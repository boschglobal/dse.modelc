package kind

import ()

const (
	ManifestKindManifest ManifestKind = "Manifest"
)
const (
	Redispubsub SimulationParametersTransport = "redispubsub"
)

type File struct {
	Generate   *string `yaml:"generate,omitempty"`
	Modelc     *bool   `yaml:"modelc,omitempty"`
	Name       string  `yaml:"name"`
	Processing *string `yaml:"processing,omitempty"`
	Repo       *string `yaml:"repo,omitempty"`
	Uri        *string `yaml:"uri,omitempty"`
}
type Manifest struct {
	Kind     ManifestKind    `yaml:"kind"`
	Metadata *ObjectMetadata `yaml:"metadata,omitempty"`
	Spec     ManifestSpec    `yaml:"spec"`
}
type ManifestKind string
type ManifestSpec struct {
	Documentation *[]File           `yaml:"documentation,omitempty"`
	Models        []ModelDefinition `yaml:"models"`
	Repos         []Repo            `yaml:"repos"`
	Simulations   []Simulation      `yaml:"simulations"`
	Tools         []Tool            `yaml:"tools"`
}
type ModelDefinition struct {
	Arch     *string    `yaml:"arch,omitempty"`
	Channels *[]Channel `yaml:"channels,omitempty"`
	Name     string     `yaml:"name"`
	Repo     string     `yaml:"repo"`
	Schema   *string    `yaml:"schema,omitempty"`
	Version  string     `yaml:"version"`
}
type ModelInstanceDefinition struct {
	Channels []Channel `yaml:"channels"`
	Files    *[]File   `yaml:"files,omitempty"`
	Model    string    `yaml:"model"`
	Name     string    `yaml:"name"`
}
type Repo struct {
	Name     string  `yaml:"name"`
	Path     *string `yaml:"path,omitempty"`
	Registry *string `yaml:"registry,omitempty"`
	Repo     *string `yaml:"repo,omitempty"`
	Token    string  `yaml:"token"`
	User     string  `yaml:"user"`
}
type Simulation struct {
	Files      *[]File                   `yaml:"files,omitempty"`
	Models     []ModelInstanceDefinition `yaml:"models"`
	Name       string                    `yaml:"name"`
	Parameters *struct {
		Environment *map[string]string            `yaml:"environment,omitempty"`
		Transport   SimulationParametersTransport `yaml:"transport"`
	} `yaml:"parameters,omitempty"`
}
type SimulationParametersTransport string
type Tool struct {
	Arch    *[]string `yaml:"arch,omitempty"`
	Name    string    `yaml:"name"`
	Repo    *string   `yaml:"repo,omitempty"`
	Schema  *string   `yaml:"schema,omitempty"`
	Version string    `yaml:"version"`
}
