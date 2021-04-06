import uuid

from src.api.public.uuidpb import uuid_pb2 as uuidpb


def uuid_pb_from_string(id_str: str) -> uuidpb.UUID:
    u = uuid.UUID(id_str)
    return uuidpb.UUID(
        high_bits=(u.int >> 64),
        low_bits=(u.int & 2**64 - 1),
    )


def uuid_pb_to_string(pb: uuidpb.UUID) -> str:
    i = (pb.high_bits << 64) + pb.low_bits
    u = uuid.UUID(int=i)
    return str(u)
