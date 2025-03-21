package kind

import (
	"encoding/json"
	"strings"

	"github.com/oapi-codegen/runtime"
)

const (
	StackKindStack StackKind = "Stack"
)

type MessageQueue struct {
	Uri *string `yaml:"uri,omitempty"`
}
type ModelInstance struct {
	Annotations *Annotations `yaml:"annotations,omitempty"`
	Channels    *[]Channel   `yaml:"channels,omitempty"`
	Model       struct {
		Mcl *struct {
			Models []struct {
				Name string `yaml:"name"`
			} `yaml:"models"`
			Strategy string `yaml:"strategy"`
		} `yaml:"mcl,omitempty"`
		Name string `yaml:"name"`
	} `yaml:"model"`
	Name    string                `yaml:"name"`
	Runtime *ModelInstanceRuntime `yaml:"runtime,omitempty"`
	Uid     int                   `yaml:"uid"`
}
type ModelInstanceRuntime struct {
	Env   *map[string]string `yaml:"env,omitempty"`
	Files *[]string          `yaml:"files,omitempty"`
	I386  *bool              `yaml:"i386,omitempty"`
	Paths *[]string          `yaml:"paths,omitempty"`
	X32   *bool              `yaml:"x32,omitempty"`
}
type RedisConnection struct {
	Timeout *int    `yaml:"timeout,omitempty"`
	Uri     *string `yaml:"uri,omitempty"`
}
type Stack struct {
	Kind     StackKind       `yaml:"kind"`
	Metadata *ObjectMetadata `yaml:"metadata,omitempty"`
	Spec     StackSpec       `yaml:"spec"`
}
type StackKind string
type StackRuntime struct {
	Env     *map[string]string `yaml:"env,omitempty"`
	Stacked *bool              `yaml:"stacked,omitempty"`
}
type StackSpec struct {
	Connection *struct {
		Timeout   *string                         `yaml:"timeout,omitempty"`
		Transport *StackSpec_Connection_Transport `yaml:"transport,omitempty"`
	} `yaml:"connection,omitempty"`
	Models  *[]ModelInstance `yaml:"models,omitempty"`
	Runtime *StackRuntime    `yaml:"runtime,omitempty"`
}
type StackSpecConnectionTransport0 struct {
	Redis RedisConnection `yaml:"redis"`
}
type StackSpecConnectionTransport1 struct {
	Redispubsub RedisConnection `yaml:"redispubsub"`
}
type StackSpecConnectionTransport2 struct {
	Mq MessageQueue `yaml:"mq"`
}
type StackSpec_Connection_Transport struct {
	union json.RawMessage
}

func (t StackSpec_Connection_Transport) AsStackSpecConnectionTransport0() (StackSpecConnectionTransport0, error) {
	var body StackSpecConnectionTransport0
	err := json.Unmarshal(t.union, &body)
	return body, err
}
func (t *StackSpec_Connection_Transport) FromStackSpecConnectionTransport0(v StackSpecConnectionTransport0) error {
	b, err := json.Marshal(v)
	t.union = b
	return err
}
func (t *StackSpec_Connection_Transport) MergeStackSpecConnectionTransport0(v StackSpecConnectionTransport0) error {
	b, err := json.Marshal(v)
	if err != nil {
		return err
	}
	merged, err := runtime.JSONMerge(t.union, b)
	t.union = merged
	return err
}
func (t StackSpec_Connection_Transport) AsStackSpecConnectionTransport1() (StackSpecConnectionTransport1, error) {
	var body StackSpecConnectionTransport1
	err := json.Unmarshal(t.union, &body)
	return body, err
}
func (t *StackSpec_Connection_Transport) FromStackSpecConnectionTransport1(v StackSpecConnectionTransport1) error {
	b, err := json.Marshal(v)
	t.union = b
	return err
}
func (t *StackSpec_Connection_Transport) MergeStackSpecConnectionTransport1(v StackSpecConnectionTransport1) error {
	b, err := json.Marshal(v)
	if err != nil {
		return err
	}
	merged, err := runtime.JSONMerge(t.union, b)
	t.union = merged
	return err
}
func (t StackSpec_Connection_Transport) AsStackSpecConnectionTransport2() (StackSpecConnectionTransport2, error) {
	var body StackSpecConnectionTransport2
	err := json.Unmarshal(t.union, &body)
	return body, err
}
func (t *StackSpec_Connection_Transport) FromStackSpecConnectionTransport2(v StackSpecConnectionTransport2) error {
	b, err := json.Marshal(v)
	t.union = b
	return err
}
func (t *StackSpec_Connection_Transport) MergeStackSpecConnectionTransport2(v StackSpecConnectionTransport2) error {
	b, err := json.Marshal(v)
	if err != nil {
		return err
	}
	merged, err := runtime.JSONMerge(t.union, b)
	t.union = merged
	return err
}
func (t StackSpec_Connection_Transport) MarshalJSON() ([]byte, error) {
	b, err := t.union.MarshalJSON()
	return b, err
}
func (t *StackSpec_Connection_Transport) UnmarshalJSON(b []byte) error {
	err := t.union.UnmarshalJSON(b)
	return err
}
func (t StackSpec_Connection_Transport) MarshalYAML() (interface{}, error) {
	b, err := t.union.MarshalJSON()
	b = []byte(strings.ToLower(string(b)))
	r := make(map[string]interface{})
	json.Unmarshal(b, &r)
	return r, err
}
