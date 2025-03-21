package animation

// Code generated by cdproto-gen. DO NOT EDIT.

// EventAnimationCanceled event for when an animation has been cancelled.
//
// See: https://chromedevtools.github.io/devtools-protocol/tot/Animation#event-animationCanceled
type EventAnimationCanceled struct {
	ID string `json:"id"` // Id of the animation that was cancelled.
}

// EventAnimationCreated event for each animation that has been created.
//
// See: https://chromedevtools.github.io/devtools-protocol/tot/Animation#event-animationCreated
type EventAnimationCreated struct {
	ID string `json:"id"` // Id of the animation that was created.
}

// EventAnimationStarted event for animation that has been started.
//
// See: https://chromedevtools.github.io/devtools-protocol/tot/Animation#event-animationStarted
type EventAnimationStarted struct {
	Animation *Animation `json:"animation"` // Animation that was started.
}
