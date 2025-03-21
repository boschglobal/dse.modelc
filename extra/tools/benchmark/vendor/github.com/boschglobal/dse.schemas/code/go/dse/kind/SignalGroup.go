package kind

import ()

const (
	SignalGroupKindSignalGroup SignalGroupKind = "SignalGroup"
)

type Signal struct {
	Annotations *Annotations `yaml:"annotations,omitempty"`
	Signal      string       `yaml:"signal"`
	Transform   *Transform   `yaml:"transform,omitempty"`
}
type SignalGroup struct {
	Kind     SignalGroupKind `yaml:"kind"`
	Metadata *ObjectMetadata `yaml:"metadata,omitempty"`
	Spec     SignalGroupSpec `yaml:"spec"`
}
type SignalGroupKind string
type SignalGroupSpec struct {
	Signals []Signal `yaml:"signals"`
}
