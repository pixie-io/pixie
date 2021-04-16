package controller_test

import (
	"testing"

	"github.com/gofrs/uuid"
	"github.com/jmoiron/sqlx"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/cloud/vzmgr/controller"
)

func mustLoadStatusMonitorTestData(db *sqlx.DB) {
	db.MustExec(`DELETE from vizier_cluster_info`)
	db.MustExec(`DELETE from vizier_cluster`)

	insertVizierClusterQuery := `INSERT INTO vizier_cluster(org_id, id) VALUES ($1, $2)`
	db.MustExec(insertVizierClusterQuery, "223e4567-e89b-12d3-a456-426655440000", "123e4567-e89b-12d3-a456-426655440000")

	insertVizierClusterInfoQuery := `INSERT INTO vizier_cluster_info(vizier_cluster_id, status, address, jwt_signing_key, last_heartbeat) VALUES($1, $2, $3, $4, NOW() - INTERVAL '30 seconds')`
	db.MustExec(insertVizierClusterInfoQuery, "123e4567-e89b-12d3-a456-426655440000", "HEALTHY", "addr0", "key0")

	insertVizierClusterQuery = `INSERT INTO vizier_cluster(org_id, id) VALUES ($1, $2)`
	db.MustExec(insertVizierClusterQuery, "223e4567-e89b-12d3-a456-426655440001", "123e4567-e89b-12d3-a456-426655440001")

	insertVizierClusterInfoQuery = `INSERT INTO vizier_cluster_info(vizier_cluster_id, status, address, jwt_signing_key, last_heartbeat) VALUES($1, $2, $3, $4, NOW() - INTERVAL '30 seconds')`
	db.MustExec(insertVizierClusterInfoQuery, "123e4567-e89b-12d3-a456-426655440001", "UPDATING", "addr0", "key0")
}

func TestStatusMonitor_Start(t *testing.T) {
	mustLoadTestData(db)
	mustLoadStatusMonitorTestData(db)

	query := `SELECT vizier_cluster_id, address, status from vizier_cluster_info WHERE vizier_cluster_id=$1`
	var vizInfo struct {
		ID      uuid.UUID `db:"vizier_cluster_id"`
		Address string    `db:"address"`
		Status  string    `db:"status"`
	}

	err := db.Get(&vizInfo, query, uuid.FromStringOrNil("123e4567-e89b-12d3-a456-426655440000"))
	require.NoError(t, err)
	assert.Equal(t, vizInfo.Address, "addr0")
	assert.Equal(t, vizInfo.Status, "HEALTHY")

	sm := controller.NewStatusMonitor(db)
	defer sm.Stop()

	// For call update, just to make sure it was run and the state was updated.
	sm.UpdateDBEntries()

	err = db.Get(&vizInfo, query, uuid.FromStringOrNil("123e4567-e89b-12d3-a456-426655440000"))
	require.NoError(t, err)
	assert.Equal(t, vizInfo.Address, "")
	assert.Equal(t, vizInfo.Status, "DISCONNECTED")

	err = db.Get(&vizInfo, query, uuid.FromStringOrNil("123e4567-e89b-12d3-a456-426655440001"))
	require.NoError(t, err)
	assert.Equal(t, vizInfo.Address, "addr0")
	assert.Equal(t, vizInfo.Status, "UPDATING")
}
