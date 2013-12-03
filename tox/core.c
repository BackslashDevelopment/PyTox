/**
 * @file   core.c
 * @author Wei-Ning Huang (AZ) <aitjcize@gmail.com>
 *
 * Copyright (C) 2013 -  Wei-Ning Huang (AZ) <aitjcize@gmail.com>
 * All Rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <Python.h>
#include <tox/tox.h>

PyObject* ToxCoreError;

static void bytes_to_hex_string(uint8_t* digest, int length,
    uint8_t* hex_digest)
{
  hex_digest[2 * length] = 0;

  int i, j;
  for(i = j = 0; i < length; ++i) {
    char c;
    c = (digest[i] >> 4) & 0xf;
    c = (c > 9) ? c + 'A'- 10 : c + '0';
    hex_digest[j++] = c;
    c = (digest[i] & 0xf);
    c = (c > 9) ? c + 'A' - 10 : c + '0';
    hex_digest[j++] = c;
  }
}

static int hex_char_to_int(char c)
{
  int val = 0;
  if (c >= '0' && c <= '9') {
    val = c - '0';
  } else if(c >= 'A' && c <= 'F') {
    val = c - 'A' + 10;
  } else if(c >= 'a' && c <= 'f') {
    val = c - 'a' + 10;
  } else {
    val = 0;
  }
  return val;
}

static void hex_string_to_bytes(uint8_t* hexstr, int length, uint8_t* bytes)
{
  int i;
  for (i = 0; i < length; ++i) {
    bytes[i] = (hex_char_to_int(hexstr[2 * i]) << 4)
             | (hex_char_to_int(hexstr[2 * i + 1]));
  }
}

/* Helper functions for Python 2 and 3 compability */

// PyString_ functions were deprecated in Python 3 in favour
// of PyUnicode_for string data and PyBytes_ for binary data:
PyObject* PyUnicodeString_FromString(const char *str) {
  PyObject* res;
  #if PY_MAJOR_VERSION < 3
    res = PyString_FromString(str);
  #else
    res = PyUnicode_FromString(str);
  #endif
  return res;
}

PyObject* PyByteString_FromString(const char *str) {
  PyObject* res;
  #if PY_MAJOR_VERSION < 3
    res = PyString_FromString(str);
  #else
    res = PyBytes_FromString(str);
  #endif
  return res;  
}

/* core.Tox definition */
typedef struct {
  PyObject_HEAD
  Tox* tox;
} ToxCore;

static void callback_friend_request(uint8_t* public_key, uint8_t* data,
    uint16_t length, void* self)
{
  uint8_t buf[TOX_FRIEND_ADDRESS_SIZE * 2 + 1];
  bytes_to_hex_string(public_key, TOX_FRIEND_ADDRESS_SIZE, buf);

  PyObject_CallMethod((PyObject*)self, "on_friend_request", "ss#", buf, data,
      length - 1);
}

static void callback_friend_message(Tox *tox, int friendnumber,
    uint8_t* message, uint16_t length, void* self)
{
  PyObject_CallMethod((PyObject*)self, "on_friend_message", "is#", friendnumber,
      message, length - 1);
}

static void callback_action(Tox *tox, int friendnumber, uint8_t* action,
    uint16_t length, void* self)
{
  PyObject_CallMethod((PyObject*)self, "on_action", "is#", friendnumber, action,
      length - 1);
}

static void callback_name_change(Tox *tox, int friendnumber, uint8_t* newname,
    uint16_t length, void* self)
{
  PyObject_CallMethod((PyObject*)self, "on_name_change", "is#", friendnumber,
      newname, length - 1);
}

static void callback_status_message(Tox *tox, int friendnumber,
    uint8_t *newstatus, uint16_t length, void* self)
{
  PyObject_CallMethod((PyObject*)self, "on_status_message", "is#", friendnumber,
      newstatus, length - 1);
}

static void callback_user_status(Tox *tox, int friendnumber,
    TOX_USERSTATUS kind, void* self)
{
  PyObject_CallMethod((PyObject*)self, "on_user_status", "ii", friendnumber,
      kind);
}

static void callback_read_receipt(Tox *tox, int friendnumber, uint32_t receipt,
    void* self)
{
  PyObject_CallMethod((PyObject*)self, "on_read_receipt", "ii", friendnumber,
      receipt);
}

static void callback_connection_status(Tox *tox, int friendnumber,
    uint8_t status, void* self)
{
  PyObject_CallMethod((PyObject*)self, "on_connection_status", "iO",
      friendnumber, PyBool_FromLong(status));
}

static void callback_group_invite(Tox *tox, int friendnumber,
    uint8_t* group_public_key, void *self)
{
  PyObject_CallMethod((PyObject*)self, "on_group_invite", "is#", friendnumber,
      group_public_key, TOX_FRIEND_ADDRESS_SIZE);
}

static void callback_group_message(Tox *tox, int groupid,
    int friendgroupid, uint8_t* message, uint16_t length, void *self)
{
  PyObject_CallMethod((PyObject*)self, "on_group_message", "iis#", groupid,
      friendgroupid, message, length - 1);
}

static void callback_group_namelist_change(Tox *tox, int groupid,
    int peernumber, uint8_t change, void* self)
{
  PyObject_CallMethod((PyObject*)self, "on_group_namelist_change", "iii",
      groupid, peernumber, change);
}

static void callback_file_send_request(Tox *m, int friendnumber,
    uint8_t filenumber, uint64_t filesize, uint8_t* filename,
    uint16_t filename_length, void* self)
{
  PyObject_CallMethod((PyObject*)self, "on_file_send_request", "iiKs#",
      friendnumber, filenumber, filesize, filename, filename_length);
}

static void callback_file_control(Tox *m, int friendnumber,
    uint8_t receive_send, uint8_t filenumber, uint8_t control_type,
    uint8_t* data, uint16_t length, void* self)
{
  PyObject_CallMethod((PyObject*)self, "on_file_control", "iiiis#",
      friendnumber, receive_send, filenumber, control_type, data, length);
}

static void callback_file_data(Tox *m, int friendnumber, uint8_t filenumber,
    uint8_t* data, uint16_t length, void* self)
{
  PyObject_CallMethod((PyObject*)self, "on_file_data", "iis#",
      friendnumber, filenumber, data, length);
}

static int init_helper(ToxCore* self, PyObject* args)
{
  if (self->tox != NULL) {
    tox_kill(self->tox);
    self->tox = NULL;
  }

  int ipv6enabled = TOX_ENABLE_IPV6_DEFAULT;

  if (args) {
    if (!PyArg_ParseTuple(args, "|i", &ipv6enabled)) {
      PyErr_SetString(PyExc_TypeError, "ipv6enabled should be boolean");
      return -1;
    }
  }

  Tox* tox = tox_new(ipv6enabled);

  tox_callback_friend_request(tox, callback_friend_request, self);
  tox_callback_friend_message(tox, callback_friend_message, self);
  tox_callback_action(tox, callback_action, self);
  tox_callback_name_change(tox, callback_name_change, self);
  tox_callback_status_message(tox, callback_status_message, self);
  tox_callback_user_status(tox, callback_user_status, self);
  tox_callback_read_receipt(tox, callback_read_receipt, self);
  tox_callback_connection_status(tox, callback_connection_status, self);
  tox_callback_group_invite(tox, callback_group_invite, self);
  tox_callback_group_message(tox, callback_group_message, self);
  tox_callback_group_namelist_change(tox, callback_group_namelist_change, self);
  tox_callback_file_send_request(tox, callback_file_send_request, self);
  tox_callback_file_control(tox, callback_file_control, self);
  tox_callback_file_data(tox, callback_file_data, self);

  self->tox = tox;

  return 0;
}

static PyObject*
ToxCore_new(PyTypeObject *type, PyObject* args, PyObject* kwds)
{
  ToxCore* self = (ToxCore*)type->tp_alloc(type, 0);
  self->tox = NULL;

  // We don't care about subclass's arguments
  if (init_helper(self, NULL) == -1) {
    return NULL;
  }

  return (PyObject*)self;
}

static int ToxCore_init(ToxCore* self, PyObject* args, PyObject* kwds)
{
  // since __init__ in Python is optional(superclass need to call it
  // explicitly), we need to initialize self->tox in ToxCore_new instead of
  // init. If ToxCore_init is called, we re-initialize self->tox and pass
  // the new ipv6enabled setting.
  return init_helper(self, args);
}

static int
ToxCore_dealloc(ToxCore* self)
{
  return 0;
}

static PyObject*
ToxCore_callback_stub(ToxCore* self, PyObject* args)
{
  Py_RETURN_NONE;
}

static PyObject*
ToxCore_getaddress(ToxCore* self, PyObject* args)
{
  uint8_t address[TOX_FRIEND_ADDRESS_SIZE];
  uint8_t address_hex[TOX_FRIEND_ADDRESS_SIZE * 2 + 1];

  tox_get_address(self->tox, address);
  bytes_to_hex_string(address, TOX_FRIEND_ADDRESS_SIZE, address_hex);

  PyObject* res = PyUnicodeString_FromString((const char*)address_hex);
  return res;
}

static PyObject*
ToxCore_add_friend(ToxCore* self, PyObject* args)
{
  uint8_t* address = NULL;
  int addr_length = 0;
  uint8_t* data = NULL;
  int data_length = 0;

  if (!PyArg_ParseTuple(args, "s#s#", &address, &addr_length, &data,
        &data_length)) {
    return NULL;
  }

  uint8_t pk[TOX_FRIEND_ADDRESS_SIZE];
  hex_string_to_bytes(address, TOX_FRIEND_ADDRESS_SIZE, pk);

  int ret = tox_add_friend(self->tox, pk, data, data_length + 1);
  int success = 0;

  switch (ret) {
  case TOX_FAERR_TOOLONG:
    PyErr_SetString(ToxCoreError, "message too long");
    break;
  case TOX_FAERR_NOMESSAGE:
    PyErr_SetString(ToxCoreError, "no message specified");
    break;
  case TOX_FAERR_OWNKEY:
    PyErr_SetString(ToxCoreError, "user's own key");
    break;
  case TOX_FAERR_ALREADYSENT:
    PyErr_SetString(ToxCoreError, "friend request already sent or already "
        "a friend");
    break;
  case TOX_FAERR_UNKNOWN:
    PyErr_SetString(ToxCoreError, "unknown error");
    break;
  case TOX_FAERR_BADCHECKSUM:
    PyErr_SetString(ToxCoreError, "bad checksum in address");
    break;
  case TOX_FAERR_SETNEWNOSPAM:
    PyErr_SetString(ToxCoreError, "the friend was already there but the "
        "nospam was different");
    break;
  case TOX_FAERR_NOMEM:
    PyErr_SetString(ToxCoreError, "increasing the friend list size fails");
    break;
  default:
    success = 1;
  }

  if (success) {
    return PyLong_FromLong(ret);
  } else {
    return NULL;
  }
}

static PyObject*
ToxCore_add_friend_norequest(ToxCore* self, PyObject* args)
{
  uint8_t* address = NULL;
  int addr_length = 0;

  if (!PyArg_ParseTuple(args, "s#", &address, &addr_length)) {
    return NULL;
  }

  uint8_t pk[TOX_FRIEND_ADDRESS_SIZE];
  hex_string_to_bytes(address, TOX_FRIEND_ADDRESS_SIZE, pk);

  if (tox_add_friend_norequest(self->tox, pk) == -1) {
    PyErr_SetString(ToxCoreError, "failed to add friend");
    return NULL;
  }

  Py_RETURN_NONE;
}

static PyObject*
ToxCore_get_friend_id(ToxCore* self, PyObject* args)
{
  uint8_t* id = NULL;
  int addr_length = 0;

  if (!PyArg_ParseTuple(args, "s#", &id, &addr_length)) {
    return NULL;
  }

  uint8_t pk[TOX_CLIENT_ID_SIZE];
  hex_string_to_bytes(id, TOX_CLIENT_ID_SIZE, pk);

  int ret = tox_get_friend_id(self->tox, pk);
  if (ret == -1) {
    PyErr_SetString(ToxCoreError, "no such friend");
    return NULL;
  }

  return PyLong_FromLong(ret);
}

static PyObject*
ToxCore_get_client_id(ToxCore* self, PyObject* args)
{
  uint8_t pk[TOX_CLIENT_ID_SIZE + 1];
  pk[TOX_FRIEND_ADDRESS_SIZE] = 0;

  uint8_t hex[TOX_CLIENT_ID_SIZE * 2 + 1];

  int friendid = 0;

  if (!PyArg_ParseTuple(args, "i", &friendid)) {
    return NULL;
  }

  tox_get_client_id(self->tox, friendid, pk);
  bytes_to_hex_string(pk, TOX_CLIENT_ID_SIZE, hex);

  return PyUnicodeString_FromString((const char*)hex);
}

static PyObject*
ToxCore_delfriend(ToxCore* self, PyObject* args)
{
  int friendid = 0;

  if (!PyArg_ParseTuple(args, "i", &friendid)) {
    return NULL;
  }

  if (tox_del_friend(self->tox, friendid) == -1) {
    PyErr_SetString(ToxCoreError, "failed to delete friend");
    return NULL;
  }

  Py_RETURN_NONE;
}

static PyObject*
ToxCore_get_friend_connection_status(ToxCore* self, PyObject* args)
{
  int friendid = 0;

  if (!PyArg_ParseTuple(args, "i", &friendid)) {
    return NULL;
  }

  int ret = tox_get_friend_connection_status(self->tox, friendid);
  if (ret == -1) {
    PyErr_SetString(ToxCoreError, "failed get connection status");
    return NULL;
  }

  return PyBool_FromLong(ret);
}

static PyObject*
ToxCore_friend_exists(ToxCore* self, PyObject* args)
{
  int friendid = 0;

  if (!PyArg_ParseTuple(args, "i", &friendid)) {
    return NULL;
  }

  int ret = tox_friend_exists(self->tox, friendid);

  return PyBool_FromLong(ret);
}

static PyObject*
ToxCore_sendmessage(ToxCore* self, PyObject* args)
{
  int friendid = 0;
  int length = 0;
  uint8_t* message = NULL;

  if (!PyArg_ParseTuple(args, "is#", &friendid, &message, &length)) {
    return NULL;
  }

  uint32_t ret = tox_send_message(self->tox, friendid, message, length + 1);
  if (ret == 0) {
    PyErr_SetString(ToxCoreError, "failed to send message");
    return NULL;
  }

  return PyLong_FromUnsignedLong(ret);
}

static PyObject*
ToxCore_sendmessage_withid(ToxCore* self, PyObject* args)
{
  int friendid = 0;
  int length = 0;
  uint32_t id = 0;
  uint8_t* message = NULL;

  if (!PyArg_ParseTuple(args, "iIs#", &friendid, &id, &message, &length)) {
    return NULL;
  }

  uint32_t ret = tox_send_message_withid(self->tox, friendid, id,message,
      length + 1);
  if (ret == 0) {
    PyErr_SetString(ToxCoreError, "failed to send message with id");
    return NULL;
  }

  return PyLong_FromUnsignedLong(ret);
}

static PyObject*
ToxCore_send_action(ToxCore* self, PyObject* args)
{
  int friendid = 0;
  int length = 0;
  uint8_t* action = NULL;

  if (!PyArg_ParseTuple(args, "is#", &friendid, &action, &length)) {
    return NULL;
  }

  uint32_t ret = tox_send_action(self->tox, friendid, action, length + 1);
  if (ret == 0) {
    PyErr_SetString(ToxCoreError, "failed to send action");
    return NULL;
  }

  return PyLong_FromUnsignedLong(ret);
}

static PyObject*
ToxCore_send_action_withid(ToxCore* self, PyObject* args)
{
  int friendid = 0;
  int length = 0;
  uint32_t id = 0;
  uint8_t* action = NULL;

  if (!PyArg_ParseTuple(args, "iIs#", &friendid, &id, &action, &length)) {
    return NULL;
  }

  uint32_t ret = tox_send_action_withid(self->tox, friendid, id, action,
      length + 1);
  if (ret == 0) {
    PyErr_SetString(ToxCoreError, "failed to send action with id");
    return NULL;
  }

  return PyLong_FromUnsignedLong(ret);
}

static PyObject*
ToxCore_set_name(ToxCore* self, PyObject* args)
{
  uint8_t* name = 0;
  int length = 0;

  if (!PyArg_ParseTuple(args, "s#", &name, &length)) {
    return NULL;
  }

  if (tox_set_name(self->tox, name, length + 1) == -1) {
    PyErr_SetString(ToxCoreError, "failed to set_name");
    return NULL;
  }

  Py_RETURN_NONE;
}

static PyObject*
ToxCore_get_self_name(ToxCore* self, PyObject* args)
{
  uint8_t buf[TOX_MAX_NAME_LENGTH];
  memset(buf, 0, TOX_MAX_NAME_LENGTH);

  if (tox_get_self_name(self->tox, buf, TOX_MAX_NAME_LENGTH) == 0) {
    PyErr_SetString(ToxCoreError, "failed to get self name");
    return NULL;
  }

  return PyUnicodeString_FromString((const char*)buf);
}

static PyObject*
ToxCore_get_name(ToxCore* self, PyObject* args)
{
  int friendid = 0;
  uint8_t buf[TOX_MAX_NAME_LENGTH];
  memset(buf, 0, TOX_MAX_NAME_LENGTH);

  if (!PyArg_ParseTuple(args, "i", &friendid)) {
    return NULL;
  }

  if (tox_get_name(self->tox, friendid, buf) == -1) {
    PyErr_SetString(ToxCoreError, "failed to get name");
    return NULL;
  }

  return PyUnicodeString_FromString((const char*)buf);
}

static PyObject*
ToxCore_set_status_message(ToxCore* self, PyObject* args)
{
  uint8_t* message;
  int length;
  if (!PyArg_ParseTuple(args, "s#", &message, &length)) {
    return NULL;
  }

  if (tox_set_status_message(self->tox, message, length + 1) == -1) {
    PyErr_SetString(ToxCoreError, "failed to set status_message");
    return NULL;
  }

  Py_RETURN_NONE;
}

static PyObject*
ToxCore_set_userstatus(ToxCore* self, PyObject* args)
{
  int status = 0;

  if (!PyArg_ParseTuple(args, "i", &status)) {
    return NULL;
  }

  if (tox_set_userstatus(self->tox, status) == -1) {
    PyErr_SetString(ToxCoreError, "failed to set status");
    return NULL;
  }

  Py_RETURN_NONE;
}


static PyObject*
ToxCore_get_status_message_size(ToxCore* self, PyObject* args)
{
  int friendid = 0;
  if (!PyArg_ParseTuple(args, "i", &friendid)) {
    return NULL;
  }

  int ret = tox_get_status_message_size(self->tox, friendid);
  return PyLong_FromLong(ret - 1);
}

static PyObject*
ToxCore_get_status_message(ToxCore* self, PyObject* args)
{
  uint8_t buf[TOX_MAX_STATUSMESSAGE_LENGTH];
  int friendid = 0;

  memset(buf, 0, TOX_MAX_STATUSMESSAGE_LENGTH);

  if (!PyArg_ParseTuple(args, "i", &friendid)) {
    return NULL;
  }

  int ret = tox_get_status_message(self->tox, friendid, buf,
      TOX_MAX_STATUSMESSAGE_LENGTH);

  if (ret == -1) {
    PyErr_SetString(ToxCoreError, "failed to get status_message");
    return NULL;
  }

  buf[TOX_MAX_STATUSMESSAGE_LENGTH -1] = 0;

  return PyUnicodeString_FromString((const char*)buf);
}

static PyObject*
ToxCore_get_self_status_message(ToxCore* self, PyObject* args)
{
  uint8_t buf[TOX_MAX_STATUSMESSAGE_LENGTH];
  memset(buf, 0, TOX_MAX_STATUSMESSAGE_LENGTH);

  int ret = tox_get_self_status_message(self->tox, buf,
      TOX_MAX_STATUSMESSAGE_LENGTH);

  if (ret == -1) {
    PyErr_SetString(ToxCoreError, "failed to get self status_message");
    return NULL;
  }

  buf[TOX_MAX_STATUSMESSAGE_LENGTH -1] = 0;

  return PyUnicodeString_FromString((const char*)buf);
}

static PyObject*
ToxCore_get_user_status(ToxCore* self, PyObject* args)
{
  int friendid = 0;
  if (!PyArg_ParseTuple(args, "i", &friendid)) {
    return NULL;
  }

  int status = tox_get_user_status(self->tox, friendid);

  return PyLong_FromLong(status);
}

static PyObject*
ToxCore_get_self_user_status(ToxCore* self, PyObject* args)
{
  int status = tox_get_self_user_status(self->tox);
  return PyLong_FromLong(status);
}

static PyObject*
ToxCore_set_send_receipts(ToxCore* self, PyObject* args)
{
  int friendid = 0;
  int yesno = 0;

  if (!PyArg_ParseTuple(args, "ii", &friendid, &yesno)) {
    return NULL;
  }

  tox_set_sends_receipts(self->tox, friendid, yesno);

  Py_RETURN_NONE;
}

static PyObject*
ToxCore_count_friendlist(ToxCore* self, PyObject* args)
{
  uint32_t count = tox_count_friendlist(self->tox);
  return PyLong_FromUnsignedLong(count);
}

static PyObject*
ToxCore_get_friendlist(ToxCore* self, PyObject* args)
{
  PyObject* plist = NULL;
  uint32_t count = tox_count_friendlist(self->tox);
  int* list = (int*)malloc(count * sizeof(int));

  uint32_t n = tox_get_friendlist(self->tox, list, count);

  if (!(plist = PyList_New(0))) {
    return NULL;
  }

  uint32_t i = 0;
  for (i = 0; i < n; ++i) {
    PyList_Append(plist, PyLong_FromLong(list[i]));
  }
  free(list);

  return plist;
}

static PyObject*
ToxCore_add_groupchat(ToxCore* self, PyObject* args)
{
  int ret = tox_add_groupchat(self->tox);
  if (ret == -1) {
    PyErr_SetString(PyExc_TypeError, "failed to add groupchat");
  }

  return PyLong_FromLong(ret);
}

static PyObject*
ToxCore_del_groupchat(ToxCore* self, PyObject* args)
{
  int groupid = 0;

  if (!PyArg_ParseTuple(args, "i", &groupid)) {
    return NULL;
  }

  if (tox_del_groupchat(self->tox, groupid) == -1) {
    PyErr_SetString(PyExc_TypeError, "failed to del groupchat");
  }

  Py_RETURN_NONE;
}

static PyObject*
ToxCore_group_peername(ToxCore* self, PyObject* args)
{
  uint8_t buf[TOX_MAX_NAME_LENGTH];
  memset(buf, 0, TOX_MAX_NAME_LENGTH);

  int groupid = 0;
  int peernumber = 0;
  if (!PyArg_ParseTuple(args, "ii", &groupid, &peernumber)) {
    return NULL;
  }

  int ret = tox_group_peername(self->tox, groupid, peernumber, buf);
  if (ret == -1) {
    PyErr_SetString(PyExc_TypeError, "failed to get group peername");
  }

  return PyUnicodeString_FromString((const char*)buf);
}

static PyObject*
ToxCore_invite_friend(ToxCore* self, PyObject* args)
{
  int friendnumber = 0;
  int groupid = 0;
  if (!PyArg_ParseTuple(args, "ii", &friendnumber, &groupid)) {
    return NULL;
  }

  if (tox_invite_friend(self->tox, friendnumber, groupid) == -1) {
    PyErr_SetString(PyExc_TypeError, "failed to invite friend");
  }

  Py_RETURN_NONE;
}

static PyObject*
ToxCore_join_groupchat(ToxCore* self, PyObject* args)
{
  uint8_t* pk = NULL;
  int length = 0;
  int friendnumber = 0;

  if (!PyArg_ParseTuple(args, "is#", &friendnumber, &pk, &length)) {
    return NULL;
  }

  int ret = tox_join_groupchat(self->tox, friendnumber, pk);
  if (ret == -1) {
    PyErr_SetString(PyExc_TypeError, "failed to join group chat");
  }

  return PyLong_FromLong(ret);
}

static PyObject*
ToxCore_group_message_send(ToxCore* self, PyObject* args)
{
  int groupid = 0;
  uint8_t* message = NULL;
  uint32_t length = 0;

  if (!PyArg_ParseTuple(args, "is#", &groupid, &message, &length)) {
    return NULL;
  }

  if (tox_group_message_send(self->tox, groupid, message, length + 1) == -1) {
    PyErr_SetString(PyExc_TypeError, "failed to send group message");
  }

  Py_RETURN_NONE;
}

static PyObject*
ToxCore_group_number_peers(ToxCore* self, PyObject* args)
{
  int groupid = 0;

  if (!PyArg_ParseTuple(args, "i", &groupid)) {
    return NULL;
  }

  int ret = tox_group_number_peers(self->tox, groupid);

  return PyLong_FromLong(ret);
}

static PyObject*
ToxCore_group_get_names(ToxCore* self, PyObject* args)
{
  int groupid = 0;
  if (!PyArg_ParseTuple(args, "i", &groupid)) {
    return NULL;
  }

  int n = tox_group_number_peers(self->tox, groupid);
  uint8_t (*names)[TOX_MAX_NAME_LENGTH] = (uint8_t(*)[TOX_MAX_NAME_LENGTH])
    malloc(sizeof(uint8_t*) * n * TOX_MAX_NAME_LENGTH);

  int n2 = tox_group_get_names(self->tox, groupid, names, n);
  if (n2 == -1) {
    PyErr_SetString(PyExc_TypeError, "failed to get group member names");
    return NULL;
  }

  PyObject* list = NULL;
  if (!(list = PyList_New(0))) {
    return NULL;
  }

  int i = 0;
  for (i = 0; i < n2; ++i) {
    PyList_Append(list, PyUnicodeString_FromString((const char*)names[i]));
  }

  free(names);

  return list;
}

static PyObject*
ToxCore_count_chatlist(ToxCore* self, PyObject* args)
{
  uint32_t n = tox_count_chatlist(self->tox);
  return PyLong_FromUnsignedLong(n);
}

static PyObject*
ToxCore_get_chatlist(ToxCore* self, PyObject* args)
{
  PyObject* plist = NULL;
  uint32_t count = tox_count_chatlist(self->tox);
  int* list = (int*)malloc(count * sizeof(int));

  int n = tox_get_chatlist(self->tox, list, count);

  if (!(plist = PyList_New(0))) {
    return NULL;
  }

  int i = 0;
  for (i = 0; i < n; ++i) {
    PyList_Append(plist, PyLong_FromLong(list[i]));
  }
  free(list);

  return plist;
}

static PyObject*
ToxCore_new_filesender(ToxCore* self, PyObject* args)
{
  int friendnumber = 0;
  int filesize = 0;
  uint8_t* filename = 0;
  int filename_length = 0;

  if (!PyArg_ParseTuple(args, "iKs#", &friendnumber, &filesize, &filename,
        &filename_length)) {
    return NULL;
  }

  int ret = tox_new_file_sender(self->tox, friendnumber, filesize, filename,
      filename_length);

  if (ret == -1) {
    PyErr_SetString(PyExc_TypeError, "tox_new_file_sender() failed");
    return NULL;
  }

  return PyLong_FromLong(ret);
}

static PyObject*
ToxCore_file_sendcontrol(ToxCore* self, PyObject* args)
{
  int friendnumber = 0;
  uint8_t send_receive = 0;
  int filenumber = 0;
  uint8_t message_id = 0;
  uint8_t* data = NULL;
  int data_length = 0;


  if (!PyArg_ParseTuple(args, "ibib|s#", &friendnumber, &send_receive,
        &filenumber, &message_id, &data, &data_length)) {
    return NULL;
  }

  int ret = tox_file_send_control(self->tox, friendnumber, send_receive,
      filenumber, message_id, data, data_length);

  if (ret == -1) {
    PyErr_SetString(PyExc_TypeError, "tox_file_send_control() failed");
    return NULL;
  }

  Py_RETURN_NONE;
}

static PyObject*
ToxCore_file_senddata(ToxCore* self, PyObject* args)
{
  int friendnumber = 0;
  int filenumber = 0;
  uint8_t* data = NULL;
  uint32_t data_length = 0;


  if (!PyArg_ParseTuple(args, "iis#", &friendnumber, &filenumber, &data,
        &data_length)) {
    return NULL;
  }

  int ret = tox_file_send_data(self->tox, friendnumber, filenumber, data,
      data_length);

  if (ret == -1) {
    PyErr_SetString(PyExc_TypeError, "tox_file_send_data() failed");
    return NULL;
  }

  Py_RETURN_NONE;
}

static PyObject*
ToxCore_file_data_size(ToxCore* self, PyObject* args)
{
  int friendnumber = 0;
  if (!PyArg_ParseTuple(args, "i", &friendnumber)) {
    return NULL;
  }

  int ret = tox_file_data_size(self->tox, friendnumber);

  if (ret == -1) {
    PyErr_SetString(PyExc_TypeError, "tox_file_data_size() failed");
    return NULL;
  }

  return PyLong_FromLong(ret);
}

static PyObject*
ToxCore_file_dataremaining(ToxCore* self, PyObject* args)
{
  int friendnumber = 0;
  int filenumber = 0;
  uint8_t send_receive = 0;


  if (!PyArg_ParseTuple(args, "iic", &friendnumber, &filenumber,
        &send_receive)) {
    return NULL;
  }

  uint64_t ret = tox_file_data_remaining(self->tox, friendnumber, filenumber,
      send_receive);

  if (!ret) {
    PyErr_SetString(PyExc_TypeError, "tox_file_data_remaining() failed");
    return NULL;
  }

  return PyLong_FromUnsignedLongLong(ret);
}

static PyObject*
ToxCore_bootstrap_from_address(ToxCore* self, PyObject* args)
{
  int ipv6enabled = TOX_ENABLE_IPV6_DEFAULT;
  int port = 0;
  uint8_t* public_key = NULL;
  char* address = NULL;
  int addr_length = 0;
  int pk_length = 0;

  if (!PyArg_ParseTuple(args, "s#iis#", &address, &addr_length, &ipv6enabled,
        &port, &public_key, &pk_length)) {
    return NULL;
  }

  int ret = tox_bootstrap_from_address(self->tox, address, ipv6enabled, port,
      public_key);

  if (!ret) {
    PyErr_SetString(ToxCoreError, "failed to resolve address");
    return NULL;
  }

  Py_RETURN_NONE;
}

static PyObject*
ToxCore_isconnected(ToxCore* self, PyObject* args)
{
  if (tox_isconnected(self->tox)) {
    Py_RETURN_TRUE;
  } else {
    Py_RETURN_FALSE;
  }
}

static PyObject*
ToxCore_kill(ToxCore* self, PyObject* args)
{
  tox_kill(self->tox);

  Py_RETURN_NONE;
}

static PyObject*
ToxCore_do(ToxCore* self, PyObject* args)
{
  tox_do(self->tox);

  Py_RETURN_NONE;
}

static PyObject*
ToxCore_size(ToxCore* self, PyObject* args)
{
  uint32_t size = tox_size(self->tox);

  return PyLong_FromUnsignedLong(size);
}

static PyObject*
ToxCore_save(ToxCore* self, PyObject* args)
{
  PyObject* res = NULL;
  int size = tox_size(self->tox);
  uint8_t* buf = (uint8_t*)malloc(size);

  if (buf == NULL) {
    return PyErr_NoMemory();
  }

  tox_save(self->tox, buf);

  // The PyString_* functions were split in Python 3.* into 
  // PyUnicode_ for string data and PyBytes_ for binary data
  #if PY_MAJOR_VERSION < 3
    res = PyString_FromStringAndSize((const char*)buf, size);
  #else
    res = PyBytes_FromStringAndSize((const char*)buf, size);
  #endif
  free(buf);

  return res;
}

static PyObject*
ToxCore_save_to_file(ToxCore* self, PyObject* args)
{
  char* filename;
  if (!PyArg_ParseTuple(args, "s", &filename)) {
    return NULL;
  }

  int size = tox_size(self->tox);
  uint8_t* buf = (uint8_t*)malloc(size);

  if (buf == NULL) {
    return PyErr_NoMemory();
  }

  tox_save(self->tox, buf);

  FILE* fp = fopen(filename, "w");

  if (fp == NULL) {
    PyErr_SetString(ToxCoreError, "tox_save(): can't open file for saving");
    return NULL;
  }

  fwrite(buf, size, 1, fp);
  fclose(fp);

  free(buf);

  Py_RETURN_NONE;
}

static PyObject*
ToxCore_load(ToxCore* self, PyObject* args)
{
  uint8_t* data = NULL;
  int length = 0;

  if (!PyArg_ParseTuple(args, "s#", &data, &length)) {
    PyErr_SetString(PyExc_TypeError, "no data specified");
    return NULL;
  }

  if (tox_load(self->tox, data, length) == -1) {
    Py_RETURN_FALSE;
  } else {
    Py_RETURN_TRUE;
  }
}

static PyObject*
ToxCore_load_from_file(ToxCore* self, PyObject* args)
{
  char* filename = NULL;

  if (!PyArg_ParseTuple(args, "s", &filename)) {
    return NULL;
  }

  FILE* fp = fopen(filename, "r");
  if (fp == NULL) {
    PyErr_SetString(PyExc_TypeError,
        "tox_load(): failed to open file for reading");
    return NULL;
  }

  size_t length = 0;
  fseek(fp, 0, SEEK_END);
  length = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  uint8_t* data = (uint8_t*)malloc(length * sizeof(char));

  if (fread(data, length, 1, fp) != 1) {
    PyErr_SetString(PyExc_TypeError,
        "tox_load(): corrupted data file");
    return NULL;
  }

  if (tox_load(self->tox, data, length) == -1) {
    Py_RETURN_FALSE;
  } else {
    Py_RETURN_TRUE;
  }
}

PyMethodDef Tox_methods[] = {
  {
    "on_friend_request", (PyCFunction)ToxCore_callback_stub, METH_VARARGS,
    "on_friend_request(address, message)\n"
    "Callback for receiving friend requests, default implementation does "
    "nothing."
  },
  {
    "on_friend_message", (PyCFunction)ToxCore_callback_stub, METH_VARARGS,
    "on_friend_message(friend_number, message)\n"
    "Callback for receiving friend messages, default implementation does "
    "nothing."
  },
  {
    "on_action", (PyCFunction)ToxCore_callback_stub, METH_VARARGS,
    "on_action(friend_number, action)\n"
    "Callback for receiving friend actions, default implementation does "
    "nothing."
  },
  {
    "on_name_change", (PyCFunction)ToxCore_callback_stub, METH_VARARGS,
    "on_name_change(friend_number, new_name)\n"
    "Callback for receiving friend name changes, default implementation does "
    "nothing."
  },
  {
    "on_status_message", (PyCFunction)ToxCore_callback_stub, METH_VARARGS,
    "on_status_message(friend_number, new_status)\n"
    "Callback for receiving friend status message changes, default "
    "implementation does nothing."
  },
  {
    "on_user_status", (PyCFunction)ToxCore_callback_stub, METH_VARARGS,
    "on_user_status(friend_number, kind)\n"
    "Callback for receiving friend status changes, default implementation "
    "does nothing.\n\n"
    ".. seealso ::\n"
    "    :meth:`.set_userstatus`"
  },
  {
    "on_read_receipt", (PyCFunction)ToxCore_callback_stub, METH_VARARGS,
    "on_read_receipt(friend_number, receipt)\n"
    "Callback for receiving read receipt, default implementation does nothing."
  },
  {
    "on_connection_status", (PyCFunction)ToxCore_callback_stub, METH_VARARGS,
    "on_connection_status(friend_number, status)\n"
    "Callback for receiving read receipt, default implementation does "
    "nothing.\n\n"
    "*status* is a boolean value which indicates the status of the friend "
    "indicated by *friend_number*. True means online and False means offline "
    "after previously online."
  },
  {
    "on_group_invite", (PyCFunction)ToxCore_callback_stub, METH_VARARGS,
    "on_group_invite(friend_number, group_public_key)\n"
    "Callback for receiving group invitations, default implementation does "
    "nothing."
  },
  {
    "on_group_message", (PyCFunction)ToxCore_callback_stub, METH_VARARGS,
    "on_group_message(group_number, friend_group_number, message)\n"
    "Callback for receiving group messages, default implementation does "
    "nothing."
  },
  {
    "on_group_namelist_change", (PyCFunction)ToxCore_callback_stub,
    METH_VARARGS,
    "on_group_namelist_change(group_number, peer_number, change)\n"
    "Callback for receiving group messages, default implementation does "
    "nothing.\n\n"
    "There are there possible *change* values:\n\n"
    "+---------------------------+----------------------+\n"
    "| change                    | description          |\n"
    "+===========================+======================+\n"
    "| Tox.CHAT_CHANGE_PEER_ADD  | a peer is added      |\n"
    "+---------------------------+----------------------+\n"
    "| Tox.CHAT_CHANGE_PEER_DEL  | a peer is deleted    |\n"
    "+---------------------------+----------------------+\n"
    "| Tox.CHAT_CHANGE_PEER_NAME | name of peer changed |\n"
    "+---------------------------+----------------------+\n"
  },
  {
    "on_file_send_request", (PyCFunction)ToxCore_callback_stub, METH_VARARGS,
    "on_file_send_request(friend_number, file_number, file_size, filename)\n"
    "Callback for receiving file send requests, default implementation does "
    "nothing."
  },
  {
    "on_file_control", (PyCFunction)ToxCore_callback_stub, METH_VARARGS,
    "on_file_control(friend_number, receive_send, file_number, control_type, "
    "data)\n"
    "Callback for receiving file send control, default implementation does "
    "nothing. See :meth:`.file_send_control` for the meaning of *receive_send* "
    "and *control_type*.\n\n"
    ".. seealso ::\n"
    "    :meth:`.file_send_control`"
  },
  {
    "on_file_data", (PyCFunction)ToxCore_callback_stub, METH_VARARGS,
    "on_file_data(friend_number, file_number, data)\n"
    "Callback for receiving file data, default implementation does nothing."
  },
  {
    "get_address", (PyCFunction)ToxCore_getaddress, METH_NOARGS,
    "get_address()\n"
    "Return address to give to others."
  },
  {
    "add_friend", (PyCFunction)ToxCore_add_friend, METH_VARARGS,
    "add_friend(address, message)\n"
    "Add a friend."
  },
  {
    "add_friend_norequest", (PyCFunction)ToxCore_add_friend_norequest,
    METH_VARARGS,
    "add_friend_norequest(address)\n"
    "Add a friend without sending request."
  },
  {
    "get_friend_id", (PyCFunction)ToxCore_get_friend_id, METH_VARARGS,
    "get_friend_id(friend_id)\n"
    "Return the friend id associated to that client id."
  },
  {
    "get_client_id", (PyCFunction)ToxCore_get_client_id, METH_VARARGS,
    "get_client_id(friend_number)\n"
    "Return the public key associated to that friend number."
  },
  {
    "del_friend", (PyCFunction)ToxCore_delfriend, METH_VARARGS,
    "del_friend(friend_number)\n"
    "Remove a friend."
  },
  {
    "get_friend_connection_status",
    (PyCFunction)ToxCore_get_friend_connection_status, METH_VARARGS,
    "get_friend_connection_status(friend_number)\n"
    "Return True if friend is connected(Online) else False."
  },
  {
    "friend_exists", (PyCFunction)ToxCore_friend_exists, METH_VARARGS,
    "friend_exists(friend_number)\n"
    "Checks if there exists a friend with given friendnumber."
  },
  {
    "send_message", (PyCFunction)ToxCore_sendmessage, METH_VARARGS,
    "send_message(friend_number, message)\n"
    "Send a text chat message to an online friend."
  },
  {
    "send_message_withid", (PyCFunction)ToxCore_sendmessage_withid,
    METH_VARARGS,
    "send_message_withid(friend_number, id, message)\n"
    "Send a text chat message to an online friend with id."
  },
  {
    "send_action", (PyCFunction)ToxCore_send_action, METH_VARARGS,
    "send_action(friend_number, action)\n"
    "Send an action to an online friend."
  },
  {
    "send_action_withid", (PyCFunction)ToxCore_send_action_withid, METH_VARARGS,
    "send_action_withid(friend_number, id, action)\n"
    "Send an action to an online friend with id."
  },
  {
    "set_name", (PyCFunction)ToxCore_set_name, METH_VARARGS,
    "set_name(name)\n"
    "Set our nickname."
  },
  {
    "get_self_name", (PyCFunction)ToxCore_get_self_name, METH_NOARGS,
    "get_self_name()\n"
    "Get your nickname."
  },
  {
    "get_name", (PyCFunction)ToxCore_get_name, METH_VARARGS,
    "get_name(friend_number)\n"
    "Get name of *friend_number*."
  },
  {
    "set_status_message", (PyCFunction)ToxCore_set_status_message, METH_VARARGS,
    "set_status_message(message)\n"
    "Set our user status message."
  },
  {
    "set_userstatus", (PyCFunction)ToxCore_set_userstatus, METH_VARARGS,
    "set_userstatus(status)\n"
    "Set our user status, status can have following values:\n\n"
    "+------------------------+--------------------+\n"
    "| kind                   | description        |\n"
    "+========================+====================+\n"
    "| Tox.USERSTATUS_NONE    | the user is online |\n"
    "+------------------------+--------------------+\n"
    "| Tox.USERSTATUS_AWAY    | the user is away   |\n"
    "+------------------------+--------------------+\n"
    "| Tox.USERSTATUS_BUSY    | the user is busy   |\n"
    "+------------------------+--------------------+\n"
    "| Tox.USERSTATUS_INVALID | invalid status     |\n"
    "+------------------------+--------------------+\n"
  },
  {
    "get_status_message_size", (PyCFunction)ToxCore_get_status_message_size,
    METH_VARARGS,
    "get_status_message_size(friend_number)\n"
    "Return the length of *friend_number*'s status message."
  },
  {
    "get_status_message", (PyCFunction)ToxCore_get_status_message,
    METH_VARARGS,
    "get_status_message(friend_number)\n"
    "Get status message of a friend."
  },
  {
    "get_self_status_message", (PyCFunction)ToxCore_get_self_status_message,
    METH_NOARGS,
    "get_self_status_message()\n"
    "Get status message of yourself."
  },
  {
    "get_user_status", (PyCFunction)ToxCore_get_user_status,
    METH_VARARGS,
    "get_user_status(friend_number)\n"
    "Get friend status.\n\n"
    ".. seealso ::\n"
    "    :meth:`.set_userstatus`"
  },
  {
    "get_self_user_status", (PyCFunction)ToxCore_get_self_user_status,
    METH_VARARGS,
    "get_self_user_status()\n"
    "Get user status of youself.\n\n"
    ".. seealso ::\n"
    "    :meth:`.set_userstatus`"
  },
  {
    "set_sends_receipts", (PyCFunction)ToxCore_set_send_receipts,
    METH_VARARGS,
    "set_sends_receipts(friend_number, yesno)\n"
    "Sets whether we send read receipts for friendnumber. *yesno* should be "
    "a boolean value."
  },
  {
    "count_friendlist", (PyCFunction)ToxCore_count_friendlist,
    METH_NOARGS,
    "count_friendlist()\n"
    "Return the number of friends."
  },
  {
    "get_friendlist", (PyCFunction)ToxCore_get_friendlist,
    METH_NOARGS,
    "get_friendlist()\n"
    "Get a list of valid friend numbers."
  },
  {
    "add_groupchat", (PyCFunction)ToxCore_add_groupchat, METH_VARARGS,
    "add_groupchat()\n"
    "Creates a new groupchat and puts it in the chats array."
  },
  {
    "del_groupchat", (PyCFunction)ToxCore_del_groupchat, METH_VARARGS,
    "del_groupchat(gruop_number)\n"
    "Delete a groupchat from the chats array."
  },
  {
    "group_peername", (PyCFunction)ToxCore_group_peername, METH_VARARGS,
    "group_peername(group_number, peer_number)\n"
    "Get the group peer's name."
  },
  {
    "invite_friend", (PyCFunction)ToxCore_invite_friend, METH_VARARGS,
    "invite_friend(friend_number, group_number)\n"
    "Invite friendnumber to groupnumber."
  },
  {
    "join_groupchat", (PyCFunction)ToxCore_join_groupchat, METH_VARARGS,
    "join_groupchat(friend_number, publick_key)\n"
    "Join a group (you need to have been invited first.)"
  },
  {
    "group_message_send", (PyCFunction)ToxCore_group_message_send, METH_VARARGS,
    "group_message_send(group_number, message)\n"
    "send a group message."
  },
  {
    "group_number_peers", (PyCFunction)ToxCore_group_number_peers, METH_VARARGS,
    "group_number_peers(group_number)\n"
    "Return the number of peers in the group chat on success."
  },
  {
    "group_get_names", (PyCFunction)ToxCore_group_get_names, METH_VARARGS,
    "group_get_names(group_number)\n"
    "List all the peers in the group chat."
  },
  {
    "count_chatlist", (PyCFunction)ToxCore_count_chatlist, METH_VARARGS,
    "count_chatlist()\n"
    "Return the number of chats in the current Tox instance."
  },
  {
    "get_chatlist", (PyCFunction)ToxCore_get_chatlist, METH_VARARGS,
    "get_chatlist()\n"
    "Return a list of valid group numbers."
  },
  {
    "new_file_sender", (PyCFunction)ToxCore_new_filesender, METH_VARARGS,
    "new_file_sender(friend_number, file_size, filename)\n"
    "Send a file send request."
  },
  {
    "file_send_control", (PyCFunction)ToxCore_file_sendcontrol, METH_VARARGS,
    "file_send_control(friend_number, send_receive, file_number, control_type"
    "[, data=None])\n"
    "Send file transfer control.\n\n"
    "*send_receive* is 0 for sending and 1 for receiving.\n\n"
    "*control_type* can be one of following value:\n\n"
    "+-------------------------------+------------------------+\n"
    "| control_type                  | description            |\n"
    "+===============================+========================+\n"
    "| Tox.FILECONTROL_ACCEPT        | accepts transfer       |\n"
    "+-------------------------------+------------------------+\n"
    "| Tox.FILECONTROL_PAUSE         | pause transfer         |\n"
    "+-------------------------------+------------------------+\n"
    "| Tox.FILECONTROL_KILL          | kill/rejct transfer    |\n"
    "+-------------------------------+------------------------+\n"
    "| Tox.FILECONTROL_FINISHED      | transfer finished      |\n"
    "+-------------------------------+------------------------+\n"
    "| Tox.FILECONTROL_RESUME_BROKEN | resume broken transfer |\n"
    "+-------------------------------+------------------------+\n"
  },
  {
    "file_send_data", (PyCFunction)ToxCore_file_senddata, METH_VARARGS,
    "file_send_data(friend_number, file_number, data)\n"
    "Send file data."
  },
  {
    "file_data_size", (PyCFunction)ToxCore_file_data_size, METH_VARARGS,
    "file_data_size(friend_number)\n"
    "Returns the recommended/maximum size of the filedata you send with "
    ":meth:`.file_send_data`."},
  {
    "file_data_remaining", (PyCFunction)ToxCore_file_dataremaining,
    METH_VARARGS,
    "file_data_remaining(friend_number, file_number, send_receive)\n"
    "Give the number of bytes left to be sent/received. *send_receive* is "
    "0 for sending and 1 for receiving."
  },
  {
    "bootstrap_from_address", (PyCFunction)ToxCore_bootstrap_from_address,
    METH_VARARGS,
    "bootstrap_from_address(address, ipv6enabled, port, public_key)\n"
    "Resolves address into an IP address. If successful, sends a 'get nodes'"
    "request to the given node with ip, port."
  },
  {
    "isconnected", (PyCFunction)ToxCore_isconnected, METH_NOARGS,
    "isconnected()\n"
    "Return False if we are not connected to the DHT."
  },
  {
    "kill", (PyCFunction)ToxCore_kill, METH_NOARGS,
    "kill()\n"
    "Run this before closing shop."
  },
  {
    "do", (PyCFunction)ToxCore_do, METH_NOARGS,
    "do()\n"
    "The main loop that needs to be run at least 20 times per second."
  },
  {
    "size", (PyCFunction)ToxCore_size, METH_NOARGS,
    "size()\n"
    "return size of messenger data (for saving)."
  },
  {
    "save", (PyCFunction)ToxCore_save, METH_NOARGS,
    "save()\n"
    "Return messenger blob in str."
  },
  {
    "save_to_file", (PyCFunction)ToxCore_save_to_file, METH_VARARGS,
    "save_to_file(filename)\n"
    "Save the messenger to a file."
  },
  {
    "load", (PyCFunction)ToxCore_load, METH_VARARGS,
    "load(blob)\n"
    "Load the messenger from *blob*."
  },
  {
    "load_from_file", (PyCFunction)ToxCore_load_from_file, METH_VARARGS,
    "load_from_file(filename)\n"
    "Load the messenger from file."
  },
  {NULL}
};

PyTypeObject ToxCoreType = {
#if PY_MAJOR_VERSION >= 3
  PyVarObject_HEAD_INIT(NULL, 0)
#else
  PyObject_HEAD_INIT(NULL)
  0,                         /*ob_size*/
#endif
  "core.Tox",                /*tp_name*/
  sizeof(ToxCore),           /*tp_basicsize*/
  0,                         /*tp_itemsize*/
  (destructor)ToxCore_dealloc, /*tp_dealloc*/
  0,                         /*tp_print*/
  0,                         /*tp_getattr*/
  0,                         /*tp_setattr*/
  0,                         /*tp_compare*/
  0,                         /*tp_repr*/
  0,                         /*tp_as_number*/
  0,                         /*tp_as_sequence*/
  0,                         /*tp_as_mapping*/
  0,                         /*tp_hash */
  0,                         /*tp_call*/
  0,                         /*tp_str*/
  0,                         /*tp_getattro*/
  0,                         /*tp_setattro*/
  0,                         /*tp_as_buffer*/
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
  "ToxCore object",          /* tp_doc */
  0,                         /* tp_traverse */
  0,                         /* tp_clear */
  0,                         /* tp_richcompare */
  0,                         /* tp_weaklistoffset */
  0,                         /* tp_iter */
  0,                         /* tp_iternext */
  Tox_methods,               /* tp_methods */
  0,                         /* tp_members */
  0,                         /* tp_getset */
  0,                         /* tp_base */
  0,                         /* tp_dict */
  0,                         /* tp_descr_get */
  0,                         /* tp_descr_set */
  0,                         /* tp_dictoffset */
  (initproc)ToxCore_init,    /* tp_init */
  0,                         /* tp_alloc */
  ToxCore_new,               /* tp_new */
};

void ToxCore_install_dict()
{
#define SET(name) \
  PyDict_SetItemString(dict, #name, PyLong_FromLong(TOX_##name));

  PyObject* dict = PyDict_New();
  SET(FAERR_TOOLONG)
  SET(FAERR_NOMESSAGE)
  SET(FAERR_OWNKEY)
  SET(FAERR_ALREADYSENT)
  SET(FAERR_UNKNOWN)
  SET(FAERR_BADCHECKSUM)
  SET(FAERR_SETNEWNOSPAM)
  SET(FAERR_NOMEM)
  SET(USERSTATUS_NONE)
  SET(USERSTATUS_AWAY)
  SET(USERSTATUS_BUSY)
  SET(USERSTATUS_INVALID)
  SET(CHAT_CHANGE_PEER_ADD)
  SET(CHAT_CHANGE_PEER_DEL)
  SET(CHAT_CHANGE_PEER_NAME)
  SET(FILECONTROL_ACCEPT)
  SET(FILECONTROL_PAUSE)
  SET(FILECONTROL_KILL)
  SET(FILECONTROL_FINISHED)
  SET(FILECONTROL_RESUME_BROKEN)

#undef SET

  ToxCoreType.tp_dict = dict;
}