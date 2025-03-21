package kind

import ()

const (
	NetworkKindNetwork NetworkKind = "Network"
)

type Network struct {
	Kind     NetworkKind     `yaml:"kind"`
	Metadata *ObjectMetadata `yaml:"metadata,omitempty"`
	Spec     NetworkSpec     `yaml:"spec"`
}
type NetworkKind string
type NetworkFunction struct {
	Annotations *Annotations `yaml:"annotations,omitempty"`
	Function    string       `yaml:"function"`
}
type NetworkFunctions struct {
	Decode *[]NetworkFunction `yaml:"decode,omitempty"`
	Encode *[]NetworkFunction `yaml:"encode,omitempty"`
}
type NetworkMessage struct {
	Annotations *Annotations      `yaml:"annotations,omitempty"`
	Functions   *NetworkFunctions `yaml:"functions,omitempty"`
	Message     string            `yaml:"message"`
	Signals     *[]NetworkSignal  `yaml:"signals,omitempty"`
}
type NetworkSignal struct {
	Annotations *Annotations `yaml:"annotations,omitempty"`
	Signal      string       `yaml:"signal"`
}
type NetworkSpec struct {
	Messages []NetworkMessage `yaml:"messages"`
}
