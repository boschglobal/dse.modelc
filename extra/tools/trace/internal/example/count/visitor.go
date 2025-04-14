// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package count

import "github.com/boschglobal/dse.modelc/extra/tools/trace/pkg/trace"

type CountVisitor struct {
	msgCount    uint32
	notifyCount uint32
}

func (c *CountVisitor) VisitChannelMsg(cm trace.ChannelMsg) {
	c.msgCount += 1
}

func (c *CountVisitor) VisitNotifyMsg(nm trace.NotifyMsg) {
	c.msgCount += 1
	c.notifyCount += 1
}
