package controller

import (
	"context"
	"fmt"
	"strings"
	"time"

	"github.com/gogo/protobuf/jsonpb"
	"github.com/googleapis/google-cloud-go-testing/storage/stiface"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	uuidpb "pixielabs.ai/pixielabs/src/api/public/uuidpb"
	"pixielabs.ai/pixielabs/src/cloud/scriptmgr/scriptmgrpb"
	pl_vispb "pixielabs.ai/pixielabs/src/shared/vispb"
)

type scriptModel struct {
	name        string
	desc        string
	pxl         string
	hasLiveView bool
}

type liveViewModel struct {
	name        string
	desc        string
	pxlContents string
	vis         *pl_vispb.Vis
}

// TODO(james): Eventually this will be a DB.
type scriptStore struct {
	Scripts   map[uuid.UUID]*scriptModel
	LiveViews map[uuid.UUID]*liveViewModel
}

// Server implements the GRPC Server for the scriptmgr service.
type Server struct {
	bundleBucket    string
	bundlePath      string
	sc              stiface.Client
	store           *scriptStore
	storeLastUpdate time.Time
	SeedUUID        uuid.UUID
}

// NewServer creates a new GRPC scriptmgr server.
func NewServer(bundleBucket string, bundlePath string, sc stiface.Client) *Server {
	s := &Server{
		bundleBucket: bundleBucket,
		bundlePath:   bundlePath,
		sc:           sc,
		store: &scriptStore{
			Scripts:   make(map[uuid.UUID]*scriptModel),
			LiveViews: make(map[uuid.UUID]*liveViewModel),
		},
		storeLastUpdate: time.Unix(0, 0),
		SeedUUID:        uuid.NewV4(),
	}
	err := s.updateStore()
	if err != nil {
		log.WithError(err).
			WithField("bucket", s.bundleBucket).
			WithField("path", s.bundlePath).
			Error("Failed to update store using bundle.json from gcs.")
	}
	return s
}

func (s *Server) addLiveView(name string, bundleScript *pixieScript) error {
	ID := uuid.NewV5(s.SeedUUID, name)

	var vis pl_vispb.Vis
	err := jsonpb.UnmarshalString(bundleScript.Vis, &vis)
	if err != nil {
		return err
	}

	s.store.LiveViews[ID] = &liveViewModel{
		name:        name,
		desc:        bundleScript.ShortDoc,
		vis:         &vis,
		pxlContents: bundleScript.Pxl,
	}

	return nil
}

func (s *Server) addScript(name string, bundleScript *pixieScript, hasLiveView bool) {
	ID := uuid.NewV5(s.SeedUUID, name)
	s.store.Scripts[ID] = &scriptModel{
		name:        name,
		desc:        bundleScript.ShortDoc,
		pxl:         bundleScript.Pxl,
		hasLiveView: hasLiveView,
	}
}

func (s *Server) updateStore() error {
	b, err := getBundle(s.sc, s.bundleBucket, s.bundlePath)
	if err != nil {
		return err
	}
	var errorMsgs []string
	for name, bundleScript := range b.Scripts {
		hasLiveView := bundleScript.Vis != ""
		s.addScript(name, bundleScript, hasLiveView)
		if hasLiveView {
			err = s.addLiveView(name, bundleScript)
			if err != nil {
				errorMsgs = append(errorMsgs, fmt.Sprintf("Error in Live View %s: %s", name, err.Error()))
			}
		}
	}

	if len(errorMsgs) > 0 {
		return fmt.Errorf("Encountered %d errors: %s", len(errorMsgs), strings.Join(errorMsgs, "\n"))
	}

	return nil
}

func (s *Server) storeUpdater() {
	t := time.NewTicker(time.Minute)
	ctx := context.Background()
	for range t.C {
		log.Trace("Checking if bundle needs updating...")
		attrs, err := s.sc.Bucket(s.bundleBucket).Object(s.bundlePath).Attrs(ctx)
		if err != nil {
			log.WithError(err).
				WithField("bucket", s.bundleBucket).
				WithField("path", s.bundlePath).
				Error("Failed to get attrs of bundle.json")
		}
		if attrs.Updated.After(s.storeLastUpdate) {
			log.Trace("Update to bundle required. Updating...")
			err := s.updateStore()
			if err != nil {
				log.WithError(err).
					WithField("bucket", s.bundleBucket).
					WithField("path", s.bundlePath).
					Error("Failed to update bundle.json from gcs.")
			}
			log.
				WithField("scripts", s.store.Scripts).
				WithField("live views", s.store.LiveViews).
				Trace("Finished updating bundle.")
			s.storeLastUpdate = attrs.Updated
		}
	}
}

func uuidToPb(u uuid.UUID) *uuidpb.UUID {
	return &uuidpb.UUID{
		Data: u.Bytes(),
	}
}

// Start starts the store updater goroutine which checks for updates to the bundle.json.
func (s *Server) Start() {
	go s.storeUpdater()
}

// GetLiveViews returns a list of all available live views.
func (s *Server) GetLiveViews(ctx context.Context, req *scriptmgrpb.GetLiveViewsReq) (*scriptmgrpb.GetLiveViewsResp, error) {
	resp := &scriptmgrpb.GetLiveViewsResp{}
	for ID, liveView := range s.store.LiveViews {
		resp.LiveViews = append(resp.LiveViews, &scriptmgrpb.LiveViewMetadata{
			Name: liveView.name,
			Desc: liveView.desc,
			ID:   uuidToPb(ID),
		})
	}
	return resp, nil
}

// GetLiveViewContents returns the pxl script, vis info, and metdata for a live view.
func (s *Server) GetLiveViewContents(ctx context.Context, req *scriptmgrpb.GetLiveViewContentsReq) (*scriptmgrpb.GetLiveViewContentsResp, error) {
	ID := uuid.FromBytesOrNil(req.LiveViewID.Data)
	if ID == uuid.Nil {
		return nil, status.Error(codes.InvalidArgument, "Invalid LiveViewID, bytes couldn't be parsed as UUID.")
	}
	liveView, ok := s.store.LiveViews[ID]
	if !ok {
		return nil, status.Errorf(codes.InvalidArgument, "LiveViewID: %s, not found.", ID.String())
	}

	return &scriptmgrpb.GetLiveViewContentsResp{
		Metadata: &scriptmgrpb.LiveViewMetadata{
			ID:   uuidToPb(ID),
			Name: liveView.name,
			Desc: liveView.desc,
		},
		PxlContents: liveView.pxlContents,
		Vis:         liveView.vis,
	}, nil
}

// GetScripts returns a list of all available scripts.
func (s *Server) GetScripts(ctx context.Context, req *scriptmgrpb.GetScriptsReq) (*scriptmgrpb.GetScriptsResp, error) {
	resp := &scriptmgrpb.GetScriptsResp{}
	for ID, script := range s.store.Scripts {
		resp.Scripts = append(resp.Scripts, &scriptmgrpb.ScriptMetadata{
			ID:          uuidToPb(ID),
			Name:        script.name,
			Desc:        script.desc,
			HasLiveView: script.hasLiveView,
		})
	}
	return resp, nil
}

// GetScriptContents returns the pxl string of the script.
func (s *Server) GetScriptContents(ctx context.Context, req *scriptmgrpb.GetScriptContentsReq) (*scriptmgrpb.GetScriptContentsResp, error) {
	ID := uuid.FromBytesOrNil(req.ScriptID.Data)
	if ID == uuid.Nil {
		return nil, status.Error(codes.InvalidArgument, "Invalid ScriptID, bytes couldn't be parsed as UUID.")
	}
	script, ok := s.store.Scripts[ID]
	if !ok {
		return nil, status.Errorf(codes.InvalidArgument, "ScriptID: %s, not found.", ID.String())
	}
	return &scriptmgrpb.GetScriptContentsResp{
		Metadata: &scriptmgrpb.ScriptMetadata{
			ID:          uuidToPb(ID),
			Name:        script.name,
			Desc:        script.desc,
			HasLiveView: script.hasLiveView,
		},
		Contents: script.pxl,
	}, nil
}
