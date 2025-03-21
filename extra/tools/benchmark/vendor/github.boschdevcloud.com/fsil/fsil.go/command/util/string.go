// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package util

func StringPtr(v string) *string {
	if len(v) > 0 {
		return &v
	} else {
		return nil
	}
}
