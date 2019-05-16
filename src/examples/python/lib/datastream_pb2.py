# Generated by the protocol buffer compiler.  DO NOT EDIT!
# source: datastream.proto

import sys
_b=sys.version_info[0]<3 and (lambda x:x) or (lambda x:x.encode('latin1'))
from google.protobuf.internal import enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from google.protobuf import reflection as _reflection
from google.protobuf import symbol_database as _symbol_database
from google.protobuf import descriptor_pb2
# @@protoc_insertion_point(imports)

_sym_db = _symbol_database.Default()




DESCRIPTOR = _descriptor.FileDescriptor(
  name='datastream.proto',
  package='sherlock',
  syntax='proto3',
  serialized_pb=_b('\n\x10\x64\x61tastream.proto\x12\x08sherlock\"4\n\x17\x44\x61taSourceFieldSelector\x12\n\n\x02id\x18\x01 \x01(\t\x12\r\n\x05value\x18\x02 \x01(\t\"\x82\x01\n\x12\x44\x61taStreamMetaData\x12\r\n\x05topic\x18\x01 \x01(\t\x12\x35\n\ncategories\x18\x02 \x03(\x0b\x32!.sherlock.DataSourceFieldSelector\x12&\n\ttopicType\x18\x03 \x01(\x0e\x32\x13.sherlock.TopicType\"g\n\x11\x44\x61taStreamMessage\x12\x11\n\ttimestamp\x18\x01 \x01(\x03\x12.\n\x08metaData\x18\x02 \x01(\x0b\x32\x1c.sherlock.DataStreamMetaData\x12\x0f\n\x07payload\x18\x03 \x01(\x0c\"X\n\x10\x44\x61taMoverMessage\x12\x16\n\x0ekafkaTimestamp\x18\x01 \x01(\x03\x12,\n\x07payload\x18\x02 \x01(\x0b\x32\x1b.sherlock.DataStreamMessage*)\n\tTopicType\x12\x08\n\x04MQTT\x10\x00\x12\x08\n\x04NATS\x10\x01\x12\x08\n\x04RTSP\x10\x02\x42\x0cZ\ndatastreamb\x06proto3')
)

_TOPICTYPE = _descriptor.EnumDescriptor(
  name='TopicType',
  full_name='sherlock.TopicType',
  filename=None,
  file=DESCRIPTOR,
  values=[
    _descriptor.EnumValueDescriptor(
      name='MQTT', index=0, number=0,
      options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='NATS', index=1, number=1,
      options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='RTSP', index=2, number=2,
      options=None,
      type=None),
  ],
  containing_type=None,
  options=None,
  serialized_start=412,
  serialized_end=453,
)
_sym_db.RegisterEnumDescriptor(_TOPICTYPE)

TopicType = enum_type_wrapper.EnumTypeWrapper(_TOPICTYPE)
MQTT = 0
NATS = 1
RTSP = 2



_DATASOURCEFIELDSELECTOR = _descriptor.Descriptor(
  name='DataSourceFieldSelector',
  full_name='sherlock.DataSourceFieldSelector',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='id', full_name='sherlock.DataSourceFieldSelector.id', index=0,
      number=1, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='value', full_name='sherlock.DataSourceFieldSelector.value', index=1,
      number=2, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=30,
  serialized_end=82,
)


_DATASTREAMMETADATA = _descriptor.Descriptor(
  name='DataStreamMetaData',
  full_name='sherlock.DataStreamMetaData',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='topic', full_name='sherlock.DataStreamMetaData.topic', index=0,
      number=1, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='categories', full_name='sherlock.DataStreamMetaData.categories', index=1,
      number=2, type=11, cpp_type=10, label=3,
      has_default_value=False, default_value=[],
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='topicType', full_name='sherlock.DataStreamMetaData.topicType', index=2,
      number=3, type=14, cpp_type=8, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=85,
  serialized_end=215,
)


_DATASTREAMMESSAGE = _descriptor.Descriptor(
  name='DataStreamMessage',
  full_name='sherlock.DataStreamMessage',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='timestamp', full_name='sherlock.DataStreamMessage.timestamp', index=0,
      number=1, type=3, cpp_type=2, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='metaData', full_name='sherlock.DataStreamMessage.metaData', index=1,
      number=2, type=11, cpp_type=10, label=1,
      has_default_value=False, default_value=None,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='payload', full_name='sherlock.DataStreamMessage.payload', index=2,
      number=3, type=12, cpp_type=9, label=1,
      has_default_value=False, default_value=_b(""),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=217,
  serialized_end=320,
)


_DATAMOVERMESSAGE = _descriptor.Descriptor(
  name='DataMoverMessage',
  full_name='sherlock.DataMoverMessage',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='kafkaTimestamp', full_name='sherlock.DataMoverMessage.kafkaTimestamp', index=0,
      number=1, type=3, cpp_type=2, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='payload', full_name='sherlock.DataMoverMessage.payload', index=1,
      number=2, type=11, cpp_type=10, label=1,
      has_default_value=False, default_value=None,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=322,
  serialized_end=410,
)

_DATASTREAMMETADATA.fields_by_name['categories'].message_type = _DATASOURCEFIELDSELECTOR
_DATASTREAMMETADATA.fields_by_name['topicType'].enum_type = _TOPICTYPE
_DATASTREAMMESSAGE.fields_by_name['metaData'].message_type = _DATASTREAMMETADATA
_DATAMOVERMESSAGE.fields_by_name['payload'].message_type = _DATASTREAMMESSAGE
DESCRIPTOR.message_types_by_name['DataSourceFieldSelector'] = _DATASOURCEFIELDSELECTOR
DESCRIPTOR.message_types_by_name['DataStreamMetaData'] = _DATASTREAMMETADATA
DESCRIPTOR.message_types_by_name['DataStreamMessage'] = _DATASTREAMMESSAGE
DESCRIPTOR.message_types_by_name['DataMoverMessage'] = _DATAMOVERMESSAGE
DESCRIPTOR.enum_types_by_name['TopicType'] = _TOPICTYPE
_sym_db.RegisterFileDescriptor(DESCRIPTOR)

DataSourceFieldSelector = _reflection.GeneratedProtocolMessageType('DataSourceFieldSelector', (_message.Message,), dict(
  DESCRIPTOR = _DATASOURCEFIELDSELECTOR,
  __module__ = 'datastream_pb2'
  # @@protoc_insertion_point(class_scope:sherlock.DataSourceFieldSelector)
  ))
_sym_db.RegisterMessage(DataSourceFieldSelector)

DataStreamMetaData = _reflection.GeneratedProtocolMessageType('DataStreamMetaData', (_message.Message,), dict(
  DESCRIPTOR = _DATASTREAMMETADATA,
  __module__ = 'datastream_pb2'
  # @@protoc_insertion_point(class_scope:sherlock.DataStreamMetaData)
  ))
_sym_db.RegisterMessage(DataStreamMetaData)

DataStreamMessage = _reflection.GeneratedProtocolMessageType('DataStreamMessage', (_message.Message,), dict(
  DESCRIPTOR = _DATASTREAMMESSAGE,
  __module__ = 'datastream_pb2'
  # @@protoc_insertion_point(class_scope:sherlock.DataStreamMessage)
  ))
_sym_db.RegisterMessage(DataStreamMessage)

DataMoverMessage = _reflection.GeneratedProtocolMessageType('DataMoverMessage', (_message.Message,), dict(
  DESCRIPTOR = _DATAMOVERMESSAGE,
  __module__ = 'datastream_pb2'
  # @@protoc_insertion_point(class_scope:sherlock.DataMoverMessage)
  ))
_sym_db.RegisterMessage(DataMoverMessage)


DESCRIPTOR.has_options = True
DESCRIPTOR._options = _descriptor._ParseOptions(descriptor_pb2.FileOptions(), _b('Z\ndatastream'))
# @@protoc_insertion_point(module_scope)
