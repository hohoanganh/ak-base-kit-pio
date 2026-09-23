/*
  HardwareSerial.cpp - Hardware serial library for Wiring
  Copyright (c) 2006 Nicholas Zambetti.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
  
  Modified 23 November 2006 by David A. Mellis
  Modified 28 September 2010 by Mark Sproul
  Modified 14 August 2012 by Alarus
  Modified 3 December 2013 by Matthijs Kooijman
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "sys_ctrl.h"

#include "Arduino.h"
#include "HardwareSerial.h"

// Public Methods //////////////////////////////////////////////////////////////
void HardwareSerial::_rx_complete_irq(char c) {
	rx_buffer_index_t i = (unsigned int)(_rx_buffer_head + 1) % SERIAL_RX_BUFFER_SIZE;

	// if we should be storing the received character into the location
	// just before the tail (meaning that the head would advance to the
	// current location of the tail), we're about to overflow the buffer
	// and so we don't write the character or advance the head.
	if (i != _rx_buffer_tail) {
		_rx_buffer[_rx_buffer_head] = c;
		_rx_buffer_head = i;
	}
}

int HardwareSerial::_tx_empty_irq(char* c) {
	// If interrupts are enabled, there must be more data in the output
	// buffer. Send the next byte
	*c = _tx_buffer[_tx_buffer_tail];
	_tx_buffer_tail = (_tx_buffer_tail + 1) % SERIAL_TX_BUFFER_SIZE;

	if (_tx_buffer_head == _tx_buffer_tail) {
		// Buffer empty, so disable interrupts
		return 0;
	}

	return 1;
}

void HardwareSerial::begin() {
	_pf_init();
}

int HardwareSerial::available(void) {
	return ((unsigned int)(SERIAL_RX_BUFFER_SIZE + _rx_buffer_head - _rx_buffer_tail)) % SERIAL_RX_BUFFER_SIZE;
}

int HardwareSerial::peek(void) {
	if (_rx_buffer_head == _rx_buffer_tail) {
		return -1;
	} else {
		return _rx_buffer[_rx_buffer_tail];
	}
}

int HardwareSerial::read(void) {
	// if the head isn't ahead of the tail, we don't have any characters
	if (_rx_buffer_head == _rx_buffer_tail) {
		return -1;
	} else {
		unsigned char c = _rx_buffer[_rx_buffer_tail];
		_rx_buffer_tail = (rx_buffer_index_t)(_rx_buffer_tail + 1) % SERIAL_RX_BUFFER_SIZE;
		return c;
	}
}

int HardwareSerial::availableForWrite(void) {
	tx_buffer_index_t head;
	tx_buffer_index_t tail;

	ENTRY_CRITICAL();
	head = _tx_buffer_head;
	tail = _tx_buffer_tail;
	EXIT_CRITICAL();

	if (head >= tail) return SERIAL_TX_BUFFER_SIZE - 1 - head + tail;
	return tail - head - 1;
}

void HardwareSerial::flush() {
	/* Cho ring TX rong that su.
	 *
	 * Ban goc de RONG - moi nguoi goi flush() deu tuong da doi xong trong khi
	 * chua doi gi ca. Vo hai voi console (ai cung chi ghi roi di tiep), nhung
	 * voi RS485 ban song cong thi la loi THAT: ha chan DE khi ring con du lieu
	 * bang cat cut khung giua chung.
	 *
	 * Ring chi voi duoc khi ISR TXE chay, nen phai co chan giong write(). */
	uint32_t guard = SERIAL_TX_FULL_GUARD;
	while (_tx_buffer_head != _tx_buffer_tail) {
		if (--guard == 0) {
			return;
		}
	}
}

size_t HardwareSerial::write(uint8_t c) {
	tx_buffer_index_t i = (_tx_buffer_head + 1) % SERIAL_TX_BUFFER_SIZE;

	/* Ring day thi cho ISR rut bot - nhung CO CHAN.
	 *
	 * Ban goc khong kiem tra gi ca: no GHI DE len byte chua kip gui.
	 * Cho vo han cung khong duoc - ai goi write() ben trong ENTRY_CRITICAL
	 * thi ngat dang tat, ISR khong the chay, ring khong bao gio voi va thiet
	 * bi treo han. Het chan thi BO byte va bao 0: mat mot byte con hon treo. */
	uint32_t guard = SERIAL_TX_FULL_GUARD;
	while (i == _tx_buffer_tail) {
		if (--guard == 0) {
			return 0;
		}
	}

	_tx_buffer[_tx_buffer_head] = c;

	ENTRY_CRITICAL();
	_tx_buffer_head = i;

	/* Bat ngat TXE NGAY TRONG critical section, va bat VO DIEU KIEN.
	 *
	 * Ban goc quyet dinh truoc vong ghi: "ring dang rong thi moi can danh
	 * thuc". Giua luc doc head/tail va luc dat head moi, ISR co the rut not
	 * byte cuoi roi TAT ngat TXE - the la byte vua ghi nam lai trong ring va
	 * khong ai danh thuc nua. Lan ghi sau thay ring KHONG rong nen cung
	 * khong danh thuc: ket cung cho den khi ring quay vong va tinh co
	 * head == tail.
	 *
	 * Dat trong critical section thi ISR khong chen vao giua duoc, va luc nay
	 * ring chac chan KHONG rong (vua ghi xong) nen bat TXE luon dung - khong
	 * co chuyen ISR rut phai byte cu roi phat ra rac.
	 *
	 * Vo hai voi console mot chieu, nen no nam im trong nen bao lau nay.
	 * Chi RS485 ban song cong moi lo ra, vi phai biet chinh xac luc nao duoc
	 * ha chan DE. Trieu chung khi dinh loi (Modbus 9600, 22/09/2026): hong
	 * theo cum DUNG 6 khung lien tiep - 256 byte ring chia 43 byte moi phan
	 * hoi - va master nhan dung 2 byte dau roi im. */
	_pf_tringger_putc();
	EXIT_CRITICAL();

	return 1;
}
