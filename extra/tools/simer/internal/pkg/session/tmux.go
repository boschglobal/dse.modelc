// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package session

// TMUX : Session, Window, Pane

type TmuxSession struct {
	Name string
	// TMUX .. single Session, contains Window, root Pane is this program output
	Window struct {
		Name  string
		Index int
	}
	Pane struct {
		Root       string
		Identifier string
	}
}

func (s *TmuxSession) Create() error {
	return nil
}

func (s *TmuxSession) Attach(c *Command) error {
	// tmux new-session -d -s SNAME -n WNAME -c ROOT

	// tmux new-window -P -t SNAME -n WNAME -c ROOT
	//  return is identifier for pane

	// tmux split-window -P -c ROOT -t identifier of first pane
	return nil
}
