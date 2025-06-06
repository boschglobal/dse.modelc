// Code generated by the FlatBuffers compiler. DO NOT EDIT.

package FlexRay

import "strconv"

type FrameChannel int8

const (
	FrameChannelNone FrameChannel = 0
	FrameChannelChA  FrameChannel = 1
	FrameChannelChB  FrameChannel = 2
	FrameChannelBoth FrameChannel = 3
)

var EnumNamesFrameChannel = map[FrameChannel]string{
	FrameChannelNone: "None",
	FrameChannelChA:  "ChA",
	FrameChannelChB:  "ChB",
	FrameChannelBoth: "Both",
}

var EnumValuesFrameChannel = map[string]FrameChannel{
	"None": FrameChannelNone,
	"ChA":  FrameChannelChA,
	"ChB":  FrameChannelChB,
	"Both": FrameChannelBoth,
}

func (v FrameChannel) String() string {
	if s, ok := EnumNamesFrameChannel[v]; ok {
		return s
	}
	return "FrameChannel(" + strconv.FormatInt(int64(v), 10) + ")"
}
