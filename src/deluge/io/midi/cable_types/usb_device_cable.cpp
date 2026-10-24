/*
 * Copyright © 2024 Synthstrom Audible Limited
 *
 * This file is part of The Synthstrom Audible Deluge Firmware.
 *
 * The Synthstrom Audible Deluge Firmware is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <https://www.gnu.org/licenses/>.
 */

#include "usb_device_cable.h"
#include "gui/l10n/l10n.h"
#include "io/debug/log.h"
#include "io/midi/midi_device.h"
#include "storage/storage_manager.h"

void MIDICableUSBUpstream::writeReferenceAttributesToFile(Serializer& writer) {
	// Same line. Usually the user wouldn't have default velocity sensitivity set for their computer.
	char const* portName = "upstreamUSB";
	switch (portNumber) {
	case 1:
		portName = "upstreamUSB2";
		break;
	case 2:
		portName = "upstreamUSB3";
		break;
	case 3:
		portName = "upstreamUSB4";
		break;
	default:
		break;
	}
	writer.writeAttribute("port", portName, false);
}

void MIDICableUSBUpstream::writeToFlash(uint8_t* memory) {
	D_PRINTLN("writing to flash port  %d  into ", portNumber);
	uint16_t vendorId = VENDOR_ID_UPSTREAM_USB;
	switch (portNumber) {
	case 1:
		vendorId = VENDOR_ID_UPSTREAM_USB2;
		break;
	case 2:
		vendorId = VENDOR_ID_UPSTREAM_USB3;
		break;
	case 3:
		vendorId = VENDOR_ID_UPSTREAM_USB4;
		break;
	default:
		break;
	}
	*(uint16_t*)memory = vendorId;
}

char const* MIDICableUSBUpstream::getDisplayName() {
	switch (portNumber) {
	case 0:
		return deluge::l10n::get(deluge::l10n::String::STRING_FOR_UPSTREAM_USB_PORT_1);
	case 1:
		return deluge::l10n::get(deluge::l10n::String::STRING_FOR_UPSTREAM_USB_PORT_2);
	case 2:
		return deluge::l10n::get(deluge::l10n::String::STRING_FOR_UPSTREAM_USB_PORT_3_SYSEX);
	case 3:
		return deluge::l10n::get(deluge::l10n::String::STRING_FOR_UPSTREAM_USB_PORT_4_SYSEX);
	default:
		return "";
	}
}
