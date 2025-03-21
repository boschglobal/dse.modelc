package kind

import ()

const (
	ModelKindModel ModelKind = "Model"
)

type ExecutableSpec struct {
	Annotations *Annotations `yaml:"annotations,omitempty"`
	Arch        *string      `yaml:"arch,omitempty"`
	Libs        *[]string    `yaml:"libs,omitempty"`
	Os          *string      `yaml:"os,omitempty"`
}
type GatewaySpec struct {
	Annotations *Annotations `yaml:"annotations,omitempty"`
}
type LibrarySpec struct {
	Annotations *Annotations `yaml:"annotations,omitempty"`
	Arch        *string      `yaml:"arch,omitempty"`
	Libs        *[]string    `yaml:"libs,omitempty"`
	Os          *string      `yaml:"os,omitempty"`
	Path        string       `yaml:"path"`
	Variant     *string      `yaml:"variant,omitempty"`
}
type Model struct {
	Kind     ModelKind       `yaml:"kind"`
	Metadata *ObjectMetadata `yaml:"metadata,omitempty"`
	Spec     ModelSpec       `yaml:"spec"`
}
type ModelKind string
type ModelSpec struct {
	Channels *[]Channel `yaml:"channels,omitempty"`
	Runtime  *struct {
		Dynlib     *[]LibrarySpec    `yaml:"dynlib,omitempty"`
		Executable *[]ExecutableSpec `yaml:"executable,omitempty"`
		Gateway    *GatewaySpec      `yaml:"gateway,omitempty"`
		Mcl        *[]LibrarySpec    `yaml:"mcl,omitempty"`
	} `yaml:"runtime,omitempty"`
}
