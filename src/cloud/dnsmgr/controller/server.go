/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package controller

import (
	"context"
	"errors"
	"fmt"
	"time"

	"github.com/gofrs/uuid"
	"github.com/jmoiron/sqlx"
	"github.com/spf13/viper"

	"px.dev/pixie/src/cloud/dnsmgr/dnsmgrenv"
	"px.dev/pixie/src/cloud/dnsmgr/dnsmgrpb"
	"px.dev/pixie/src/utils"
)

// ResourceRecordTTL is the TTL of the resource record in seconds.
const ResourceRecordTTL = 30

// Server is an implementation of GRPC server for dnsmgr service.
type Server struct {
	env        dnsmgrenv.DNSMgrEnv
	dnsService DNSService
	db         *sqlx.DB
}

// SSLCert represents an ssl cert in our system.
type SSLCert struct {
	CName     string     `db:"cname"`
	ClusterID *uuid.UUID `db:"cluster_id"`
	Cert      string     `db:"cert"`
	Key       string     `db:"key"`
}

// NewServer creates a new GRPC dnsmgr server.
func NewServer(env dnsmgrenv.DNSMgrEnv, dnsService DNSService, db *sqlx.DB) *Server {
	return &Server{env: env, dnsService: dnsService, db: db}
}

func (s *Server) createSSLCert(clusterID uuid.UUID) (*SSLCert, error) {
	query := `UPDATE ssl_certs SET cluster_id=$1
		WHERE cname =(SELECT cname FROM ssl_certs WHERE cluster_id IS NULL ORDER BY cname LIMIT 1) RETURNING *`

	var val SSLCert

	rows, err := s.db.Queryx(query, clusterID)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	if rows.Next() {
		err := rows.StructScan(&val)
		if err != nil {
			return nil, err
		}
		return &val, nil
	}
	return nil, errors.New("Could not read ssl_cert")
}

func (s *Server) getClusterSSLCert(clusterID uuid.UUID) (*SSLCert, error) {
	query := `SELECT * from ssl_certs WHERE cluster_id=$1`
	var val SSLCert

	rows, err := s.db.Queryx(query, clusterID)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	if rows.Next() {
		err := rows.StructScan(&val)
		if err != nil {
			return nil, err
		}
		return &val, nil
	}
	return nil, nil
}

func (s *Server) getDefaultSSLCert() (*SSLCert, error) {
	query := `SELECT cname, cert, key from ssl_certs WHERE cname='default'`

	rows, err := s.db.Queryx(query)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var val SSLCert

	if rows.Next() {
		err := rows.StructScan(&val)
		if err != nil {
			return nil, err
		}
		return &val, nil
	}
	return nil, nil
}

func (s *Server) getDNSAddress(cname string, timeCreated time.Time) string {
	timeStr := timeCreated.UnixNano()

	domainName := viper.GetString("domain_name")
	return fmt.Sprintf("%d.%s.clusters.%s", timeStr, cname, domainName)
}

func (s *Server) getTimeCreatedForClusterIP(clusterID uuid.UUID, ip string) (time.Time, error) {
	query := `SELECT time_created from dns_addresses WHERE cluster_id=$1 AND address=$2`
	var val struct {
		TimeCreated *time.Time `db:"time_created"`
	}

	rows, err := s.db.Queryx(query, clusterID, ip)
	if err != nil {
		return time.Time{}, err
	}
	defer rows.Close()

	if rows.Next() {
		err := rows.StructScan(&val)
		if err != nil {
			return time.Time{}, err
		}
		return *val.TimeCreated, nil
	}
	return time.Time{}, nil
}

func (s *Server) createDNSAddressForClusterIP(clusterID uuid.UUID, ip string) (time.Time, error) {
	query := `
		INSERT INTO dns_addresses (cluster_id, address, time_created) VALUES ($1, $2, NOW()) RETURNING time_created`
	row, err := s.db.Queryx(query, clusterID, ip)
	if err != nil {
		return time.Time{}, err
	}
	defer row.Close()

	if row.Next() {
		var timeCreated time.Time

		err := row.Scan(&timeCreated)
		if err != nil {
			return time.Time{}, err
		}
		return timeCreated, nil
	}
	return time.Time{}, errors.New("failed to read dns_address from database")
}

// GetDNSAddress assigns and returns a DNS address for the given cluster ID and IP.
func (s *Server) GetDNSAddress(ctx context.Context, req *dnsmgrpb.GetDNSAddressRequest) (*dnsmgrpb.GetDNSAddressResponse, error) {
	if s.dnsService == nil {
		return nil, errors.New("No/invalid credentials provided for DNSService")
	}

	clusterID, err := utils.UUIDFromProto(req.ClusterID)
	if err != nil {
		return nil, err
	}

	var cname string
	useDefault := viper.GetBool("use_default_dns_cert")
	if useDefault {
		cname = "default"
	} else {
		cert, err := s.getClusterSSLCert(clusterID)
		if err != nil {
			return nil, err
		}
		if cert == nil {
			return nil, errors.New("Cluster does not have a cert")
		}
		cname = cert.CName
	}

	timeCreated, err := s.getTimeCreatedForClusterIP(clusterID, req.IPAddress)
	if err != nil {
		return nil, err
	}
	if !timeCreated.IsZero() {
		return &dnsmgrpb.GetDNSAddressResponse{
			DNSAddress: s.getDNSAddress(cname, timeCreated),
		}, nil
	}

	// If clusterID and address don't already exist in database, create a new record.
	timeCreated, err = s.createDNSAddressForClusterIP(clusterID, req.IPAddress)
	if err != nil {
		return nil, err
	}

	dnsAddr := s.getDNSAddress(cname, timeCreated)

	err = s.dnsService.CreateResourceRecord(dnsAddr+".", req.IPAddress, ResourceRecordTTL)
	if err != nil {
		return nil, err
	}

	return &dnsmgrpb.GetDNSAddressResponse{
		DNSAddress: dnsAddr,
	}, nil
}

// GetSSLCerts gets a ssl cert for the given cluster ID.
func (s *Server) GetSSLCerts(ctx context.Context, req *dnsmgrpb.GetSSLCertsRequest) (*dnsmgrpb.GetSSLCertsResponse, error) {
	useDefault := viper.GetBool("use_default_dns_cert")
	if useDefault {
		cert, err := s.getDefaultSSLCert()
		if err != nil {
			return nil, err
		}
		if cert == nil {
			return nil, errors.New("Could not get default SSL cert")
		}
		return &dnsmgrpb.GetSSLCertsResponse{
			Key:  cert.Key,
			Cert: cert.Cert,
		}, nil
	}

	clusterID, err := utils.UUIDFromProto(req.ClusterID)
	if err != nil {
		return nil, err
	}
	cert, err := s.getClusterSSLCert(clusterID)
	if err != nil {
		return nil, err
	}

	if cert != nil {
		return &dnsmgrpb.GetSSLCertsResponse{
			Key:  cert.Key,
			Cert: cert.Cert,
		}, nil
	}

	cert, err = s.createSSLCert(clusterID)
	if err != nil {
		return nil, err
	}

	return &dnsmgrpb.GetSSLCertsResponse{
		Key:  cert.Key,
		Cert: cert.Cert,
	}, nil
}
