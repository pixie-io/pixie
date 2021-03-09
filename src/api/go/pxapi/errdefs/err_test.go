package errdefs

import "testing"

func TestIsInternalErr(t *testing.T) {
	if IsInternalError(ErrClusterNotFound) {
		t.Fatalf("ErrClusterNotFound is not an internal error")
	}

	if !IsInternalError(ErrInternal) {
		t.Fatalf("ErrInternal should be an internal error")
	}

	if !IsInternalError(ErrInternalUnImplementedType) {
		t.Fatalf("ErrInternalUnImplementedType should be an internal error")
	}
}

func TestIsCompilationErr(t *testing.T) {
	if IsCompilationError(ErrClusterNotFound) {
		t.Fatalf("ErrClusterNotFound is not a compilation error")
	}

	if !IsCompilationError(ErrCompilation) {
		t.Fatalf("ErrCompilation should be an compilation error")
	}
}

func TestErrorGroup(t *testing.T) {
	errs := newErrorGroup(ErrInternal, ErrInternalUnImplementedType)
	expectedMsg := `Multiple Errors: internal error, unimplemented type : internal error`
	if errs.Error() != expectedMsg {
		t.Fatalf("expected message to be %v, got %v", errs.Error(), expectedMsg)
	}
	eg, ok := errs.(errorGroup)
	if !ok {
		t.Fatal("should be an error group")
	}
	if len(eg.Errors()) != 2 {
		t.Fatal("should have exactly two errors")
	}
	if eg.Errors()[0] != ErrInternal {
		t.Fatal("should be ErrInternal")
	}
	if eg.Errors()[1] != ErrInternalUnImplementedType {
		t.Fatal("should be ErrInternalUnimplementedType")
	}
}
