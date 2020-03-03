package controller

import (
	"context"
	"fmt"

	"github.com/olivere/elastic/v7"
	log "github.com/sirupsen/logrus"
)

// IndexManager manages an index in elastic.
type IndexManager struct {
	es *elastic.Client
}

// NewIndexManager creates a manager with the elastic client.
func NewIndexManager(es *elastic.Client) *IndexManager {
	return &IndexManager{
		es: es,
	}
}

func (im *IndexManager) createIndex(indexName, mapping string) error {
	_, err := im.es.CreateIndex(indexName).Body(mapping).Do(context.Background())
	return err
}

func (im *IndexManager) hasIndex(indexName string) (bool, error) {
	return im.es.IndexExists(indexName).Do(context.Background())
}

// PrepareIndex will create an index if it does not exist or error out if it does.
func (im *IndexManager) PrepareIndex(index, mapping string) error {
	exists, err := im.hasIndex(index)
	if err != nil {
		return err
	}
	if exists {
		return fmt.Errorf("index '%s' already exists, cannot prepare it", index)
	}

	return im.createIndex(index, mapping)
}

// Reindex copies data from the src index to the dest. Both indices must exist.
func (im *IndexManager) Reindex(src string, dest string) error {
	srcIdx := elastic.NewReindexSource().Index(src)
	destIdx := elastic.NewReindexDestination().Index(dest)
	resp, err := im.es.Reindex().Source(srcIdx).Destination(destIdx).Refresh("true").Do(context.Background())
	if err != nil {
		return err
	}
	log.WithField("resp", resp).Infof("reindex result")
	return nil
}
