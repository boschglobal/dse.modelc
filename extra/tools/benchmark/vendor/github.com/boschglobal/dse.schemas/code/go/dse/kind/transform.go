package kind

type Transform struct {
	Linear *struct {
		Factor float64 `yaml:"factor"`
		Offset float64 `yaml:"offset"`
	} `yaml:"linear,omitempty"`
}
