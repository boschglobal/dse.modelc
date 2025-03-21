package kind

import ()

type Channel struct {
	Alias              *string      `yaml:"alias,omitempty"`
	Annotations        *Annotations `yaml:"annotations,omitempty"`
	ExpectedModelCount *int         `yaml:"expectedModelCount,omitempty"`
	Name               *string      `yaml:"name,omitempty"`
	Selectors          *Labels      `yaml:"selectors,omitempty"`
}
