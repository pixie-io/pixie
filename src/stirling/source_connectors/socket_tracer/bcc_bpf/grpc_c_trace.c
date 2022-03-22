/*
 * @brief   This eBPF module traces the multi-lingual gRPC library, that from now on will
 *          be referred to as "grpc-c".
 *          This module uses multiple tracepoints across the grpc-c library to capture
 *          the HTTP2 headers and data being sent and received.
 * @detailed
 *          gRPC has to be seen in the user-mode library that handles it, instead of in the
 *          generic read/write tracepoints, because gRPC is transmitted over HTTP2, which
 *          uses a header-compression mechanism called HPack. The HPack mechanism renders
 *          the normal read/write tracepoints useless, unless the connection was traced
 *          since its beginning, which we can't assume.
 *          The gRPC-c library is the main library that handles the gRPC protocol for many
 *          known languages: Python, Ruby, C, C++, and more.
 *          This module traces the library in the following locations:
 *          -   Read
 *                  The function "grpc_chttp2_data_parser_parse" is traced to see the data
 *                  received by the library.
 *                  On each call to this function, a single slice of data is received.
 *                  The headers accompanied are seen in two ways:
 *                  1.  Some of the headers are readable during this functions.
 *                  2.  Most of the headers are traced in other functions,
 *                      "grpc_chttp2_maybe_complete_recv_initial_metadata" and
 *                      "grpc_chttp2_maybe_complete_recv_trailing_metadata".
 *          -   Write
 *                  The function "grpc_chttp2_list_pop_writable_data" is traced to see the data
 *                  being sent by the library.
 *                  On each call, multiple data slices may be sent.
 *                  Therefore, there may be multiple perf events by a single call to this function.
 *                  The headers accompanied are seen during this function, and will be attached
 *                  to the first perf event.
 *          -   Close
 *                  The function "grpc_chttp2_mark_stream_closed" is traced to notice when
 *                  an HTTP2 stream is closed for writing and/or reading.
 *                  A call to this function can be made to close the stream for writing, reading
 *                  or both.
 *                  The event created is the same - a single event can notify the closing of
 *                  the stream for both reading and writing, or for just one of them.
 *          The data is passed from this eBPF module to the user mode in the following manner:
 *          -   The following perf buffers are used:
 *              1.  gRPC-C events
 *              2.  gRPC-C header events
 *              3.  gRPC-C Stream close events
 *
 * @usage   -   Register on the perf buffers that the module sends data to.
 *          -   Upon receiving an event, the associated data will be in the perf buffer,
 *              as stated above.
 *          -   To start using the library, attach the aforementioned hooks in their relevant
 *              locations.
 *          -   On newer versions of the grpc-c library, the library is stripped and the
 *              addresses of the functions we trace should be found in another manner.
 *          -   Before attaching the uprobes, set the version of the library to the
 *              versions map. This module is version-dependent, and can only trace the library
 *              of known versions.
 *          -   Before using the module, initialize its percpu variables (used as "heap").
 *              1.  grpc_c_event_data_local
 *              2.  grpc_c_header_event_data_local
 *              2.  grpc_c_metadata_local
 */

#include <linux/sched.h>
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/grpc_c.h"
#include "src/stirling/bpf_tools/bcc_bpf/task_struct_utils.h"

BPF_PERF_OUTPUT(grpc_c_events);
BPF_PERF_OUTPUT(grpc_c_header_events);
BPF_PERF_OUTPUT(grpc_c_close_events);

struct list_pop_writable_stream_arguments
{
    void * transport;
    void ** stream;
};

BPF_PERCPU_ARRAY(list_pop_writable_stream_arguments_local, struct list_pop_writable_stream_arguments, 1);
BPF_PERCPU_ARRAY(grpc_c_metadata_local, struct grpc_c_metadata_t, 1);
BPF_PERCPU_ARRAY(grpc_c_header_event_data_local, struct grpc_c_header_event_data_t, 1);
BPF_PERCPU_ARRAY(grpc_c_event_data_local, struct grpc_c_event_data_t, 1);

// Maps process id to version (@see grpc_c_version_e).
BPF_HASH(grpc_c_versions, uint32_t, uint64_t, GRPC_C_DEFAULT_MAP_SIZE);

// Size of a single slice is 0x20.
// The refcount pointer is 0x8 bytes, then the union takes the amount of bytes needed for the larger option.
// The larger option is the inlined slice, for which 0x18 bytes are needed.
#define GRPC_SLICE_SIZE (0x20)

// Types of structs in the grpc-c library.
typedef void grpc_slice_buffer;
typedef void grpc_slice;
typedef void grpc_chttp2_transport;
typedef void grpc_chttp2_stream;
typedef void grpc_endpoint;
typedef void grpc_metadata_batch;
typedef void grpc_mdelem_list; // mdelem is short for metadata element
typedef void grpc_linked_mdelem;
typedef void grpc_mdelem_data;

/*
 * @brief   Initiate an empty grpc metadata struct.
 *
 * @return  NULL on failure - if user mode did not previously initiate our
 *          heap.
 *          Otherwise, a metadata struct on our percpu "heap".
 */
static inline struct grpc_c_metadata_t * initiate_empty_grpc_metadata()
{
    u32 zero = 0;

    struct grpc_c_metadata_t * metadata = grpc_c_metadata_local.lookup(&zero);
    if (NULL == metadata)
    {
        // User mode did not initiate the percpu buffer.
        return NULL;
    }

#pragma unroll
    for (u16 i = 0 ; i < MAXIMUM_AMOUNT_OF_ITEMS_IN_METADATA ; i++)
    {
        #pragma unroll
        for (u16 j = 0 ; j < MAXIMUM_LENGTH_OF_VALUE_IN_METADATA ; j++)
        {
            ((volatile struct grpc_c_metadata_t *)metadata)->items[i].value[j] = 0;
        }
        #pragma unroll
        for (u16 j = 0 ; j < MAXIMUM_LENGTH_OF_KEY_IN_METADATA ; j++)
        {
            ((volatile struct grpc_c_metadata_t *)metadata)->items[i].key[j] = 0;
        }
    }
    metadata->count = 0;

    return metadata;
}

/*
 * @brief   Initiate an gRPC data event.
 *
 * @return  NULL on failure - if user mode did not previously initiate our
 *          heap.
 *          Otherwise, a gRPC event struct on our percpu "heap".
 */
static inline struct grpc_c_event_data_t * initiate_empty_grpc_event_data()
{
    u32 zero = 0;

    struct grpc_c_event_data_t * data = grpc_c_event_data_local.lookup(&zero);
    if (NULL == data)
    {
        // User mode did not initiate the percpu buffer.
        return NULL;
    }

    data->stream_id = 0;
    data->timestamp = 0;
    data->stack_id = 0;
    data->direction = GRPC_C_EVENT_DIRECTION_UNKNOWN;
    data->position_in_stream = 0;
    data->slice.slice_len = 0;

    // Use the struct as volatile so that the compiler doesn't optimize this loop
    // to a memset, which isn't supported in eBPF.
#pragma unroll
    for (u16 i = 0 ; i < GRPC_C_SLICE_SIZE; i++)
    {
        ((volatile struct grpc_c_event_data_t *)(data))->slice.bytes[i] = 0;
    }

    return data;
}

/*
 * @brief   Initiate an empty header event.
 *
 * @return  NULL on failure - if user mode did not previously initiate our
 *          heap.
 *          Otherwise, a header event struct on our percpu "heap".
 */
static inline struct grpc_c_header_event_data_t * initiate_empty_grpc_header_event_data()
{
    u32 zero = 0;

    struct grpc_c_header_event_data_t * data = grpc_c_header_event_data_local.lookup(&zero);
    if (NULL == data)
    {
        // User mode did not initiate the percpu buffer.
        return NULL;
    }

    data->stream_id = 0;
    data->timestamp = 0;
    data->stack_id = 0;
    data->direction = GRPC_C_EVENT_DIRECTION_UNKNOWN;

    struct grpc_c_metadata_item_t * metadata_item = &(data->header);
#pragma unroll
    for (u16 i = 0 ; i < MAXIMUM_LENGTH_OF_VALUE_IN_METADATA ; i++)
    {
        ((volatile struct grpc_c_metadata_item_t *)metadata_item)->value[i] = 0;
    }
#pragma unroll
    for (u16 i = 0 ; i < MAXIMUM_LENGTH_OF_KEY_IN_METADATA ; i++)
    {
        ((volatile struct grpc_c_metadata_item_t *)metadata_item)->key[i] = 0;
    }

    return data;
}

/*
 * @brief   Find the version of the process by its process id.
 *
 * @param   pid - the process id of which we need to find the version.
 *
 * @return  The version of the process, that was set beforehand by the user-mode.
 *          On error, GRPC_C_VERSION_UNSUPPORTED, (on the following cases)
 *          -   The version for this process was not set by the user-mode.
 *          -   An invalid version was set by the user-mode.
 */
static inline u64 lookup_version(u32 pid)
{
    u64 * version = grpc_c_versions.lookup(&pid);
    if (NULL == version)
    {
        // Version was not previously set by user-mode as it should have been.
        return GRPC_C_VERSION_UNSUPPORTED;
    }

    if (*version >= GRPC_C_VERSION_LAST)
    {
        // The version in the map is invalid.
        return GRPC_C_VERSION_UNSUPPORTED;
    }

    return *version;
}

/*
 * @brief   Read a pointer from the tracee's memory.
 *
 * @param   src         The location from which we read the pointer.
 * @param   offset      Offset from src where the pointer is at.
 * @param   dst         Where the read pointer will be stored.
 *
 * @return  0 on success, otherwise on failure.
 */
static inline u32 dereference_at(void * src, const u32 offset, /* OUT */ void ** dst)
{
    if (NULL == src || NULL == dst)
    {
        return -1;
    }

    if (0 != bpf_probe_read(
            dst,
            sizeof(*dst),
            (void *)(src + offset)))
    {
        return -1;
    }

    return 0;
}

/*
 * @brief   Read a stream id.
 * @detailed
 *          The stream id is stored as a single integer inside the stream struct.
 *
 * @param   stream          The pointer to the stream object in the tracee's memory.
 * @param   stream_id       Where the stream id will be stored.
 * @param   version         The version of the grpc-c library.
 *
 * @return  0 on success, otherwise on failure.
 */
static inline u32 get_stream_id(
    grpc_chttp2_stream * stream,
    /* OUT */ u32 * stream_id,
    const u64 version)
{
    u32 offset = 0;

    if (NULL == stream_id || NULL == stream_id)
    {
        return -1;
    }

    switch (version)
    {
        case GRPC_C_V1_19_0:
            offset = 0xa0;
            break;
        case GRPC_C_V1_24_1:
            offset = 0xa8;
            break;
        case GRPC_C_V1_33_2:
        case GRPC_C_V1_41_1:
            offset = 0xa0;
            break;
        default:
            return -1;
    }

    if (0 != bpf_probe_read(
            stream_id,
            sizeof(*stream_id),
            (void *)(stream + offset)))
    {
        return -1;
    }

    return 0;
}

/*
 * @brief   Read a file descriptor.
 * @detailed
 *          The transport struct has an endpoint member, which is used to communicate
 *          (send and receive data to and from the endpoint). This endpoint is in
 *          transport+0x10.
 *          When the endpoint is a TCP endpoint, the file descriptor is stored at
 *          endpoint+0x10.
 *
 * @param   transport   The pointer to the transport object in the tracee's memory.
 * @param   fd          Where the file descriptor will be stored.
 *
 * @remark  This function assumes that the protocol upon which the gRPC traffic is
 *          transported is TCP. Other implementations are not supported, and if
 *          another protocol is used, the behavior of this function is undefined.
 *
 * @return  0 on success, otherwise on failure.
 */
static inline u32 get_fd_from_transport(
    grpc_chttp2_transport * transport,
    /* OUT */ u32 * fd)
{
    grpc_endpoint * endpoint = NULL;

    if (NULL == fd || NULL == transport)
    {
        return -1;
    }

    if (0 != bpf_probe_read(
            &endpoint,
            sizeof(endpoint),
            (void *)(transport + 0x10)))
    {
        return -1;
    }
    if (NULL == endpoint)
    {
        return -1;
    }

    // If endpoint is a grpc_tcp object (we assume it's the case), offset is 0x10.
    if (0 != bpf_probe_read(
            fd,
            sizeof(*fd),
            (void *)(endpoint + 0x10)))
    {
        return -1;
    }

    return 0;
}

/*
 * @brief   Get the initial metadata received for the stream.
 *
 * @param   stream      The stream of which the initial metadata is to be found.
 * @param   initial_metadata_batch
 *                      The metadata batch pointer to be filled.
 * @param   version     The version of the grpc-c library
 *
 * @return  0 on success, otherwise on failure.
 */
static inline u32 get_recv_initial_metadata_batch_from_stream(
    grpc_chttp2_stream * stream,
    /* OUT */ grpc_metadata_batch ** initial_metadata_batch,
    const u64 version)
{
    uint32_t offset = 0;
    switch (version)
    {
        case GRPC_C_V1_19_0:
            offset = 0x140;
            break;
        case GRPC_C_V1_24_1:
            offset = 0x148;
            break;
        case GRPC_C_V1_33_2:
        case GRPC_C_V1_41_1:
            offset = 0x140;
            break;
        default:
            return -1;
    }
    return dereference_at(stream, offset, initial_metadata_batch);
}

/*
 * @brief   Get the trailing metadata received for the stream.
 *
 * @param   stream      The stream of which the trailing metadata is to be found.
 * @param   trailing_metadata_batch
 *                      The metadata batch pointer to be filled.
 * @param   version     The version of the grpc-c library
 *
 * @return  0 on success, otherwise on failure.
 */
static inline u32 get_recv_trailing_metadata_batch_from_stream(
    grpc_chttp2_stream * stream,
    /* OUT */ grpc_metadata_batch ** trailing_metadata_batch,
    const u64 version)
{
    uint32_t offset = 0;
    switch (version)
    {
        case GRPC_C_V1_19_0:
            offset = 0x168;
            break;
        case GRPC_C_V1_24_1:
            offset = 0x170; // check this?
            break;
        case GRPC_C_V1_33_2:
            offset = 0x168;
            break;
        case GRPC_C_V1_41_1:
            offset = 0x170;
            break;
        default:
            return -1;
    }
    return dereference_at(stream, offset, trailing_metadata_batch);
}

/*
 * @brief   Get the initial metadata sent by the stream.
 *
 * @param   stream      The stream of which the initial metadata is to be found.
 * @param   initial_metadata_batch
 *                      The metadata batch pointer to be filled.
 * @param   version     The version of the grpc-c library
 *
 * @return  0 on success, otherwise on failure.
 */
static inline u32 get_send_initial_metadata_batch_from_stream(
    grpc_chttp2_stream * stream,
    /* OUT */ grpc_metadata_batch ** initial_metadata_batch,
    const u64 version)
{
    uint32_t offset = 0;
    switch (version)
    {
        case GRPC_C_V1_19_0:
            offset = 0xa8;
            break;
        case GRPC_C_V1_24_1:
            offset = 0xb0;
            break;
        case GRPC_C_V1_33_2:
        case GRPC_C_V1_41_1:
            offset = 0xa8;
            break;
        default:
            return -1;
    }
    return dereference_at(stream, offset, initial_metadata_batch);
}

/*
 * @brief   Get the trailing metadata sent by the stream.
 *
 * @param   stream      The stream of which the trailing metadata is to be found.
 * @param   initial_metadata_batch
 *                      The metadata batch pointer to be filled.
 * @param   version     The version of the grpc-c library
 *
 * @return  0 on success, otherwise on failure.
 */
static inline u32 get_send_trailing_metadata_batch_from_stream(
    grpc_chttp2_stream * stream,
    /* OUT */ grpc_metadata_batch ** trailing_metadata_batch,
    const u64 version)
{
    uint32_t offset = 0;
    switch (version)
    {
        case GRPC_C_V1_19_0:
            offset = 0xb8;
            break;
        case GRPC_C_V1_24_1:
            offset = 0xc0;
            break;
        case GRPC_C_V1_33_2:
        case GRPC_C_V1_41_1:
            offset = 0xb8;
            break;
        default:
            return -1;
    }
    return dereference_at(stream, offset, trailing_metadata_batch);
}

/*
 * @brief   Get the length and data pointer of a slice in the tracee's memory.
 *
 * @param   slice       The slice pointer in the tracee's memory.
 * @param   length      Where the length of the slice is to be filled.
 * @param   bytes       Where the slice's data pointer is to be filled.
 *
 * @return  0 on success, otherwise on failure.
 */
static inline u32 get_data_ptr_from_slice(
    grpc_slice * slice,
    /* OUT */ u32 * length,
    /* OUT */ void ** bytes)
{
    void * refcount = NULL;

    if (NULL == slice || NULL == length || NULL == bytes)
    {
        return -1;
    }

    // Read refcount
    if (0 != bpf_probe_read(
            &refcount,
            sizeof(refcount),
            (void *)(slice)))
    {
        return -1;
    }
    if (unlikely(NULL == refcount))
    {
        // This slice is an inlined grpc slice.
        // Bytes are directly inside the slice object.
        if (0 != bpf_probe_read(
                length,
                sizeof(u8),
                (void *)(slice + 0x8)))
        {
            return -1;
        }
        *bytes = slice + 0x9;
        return 0;
    }

    // Read length
    if (0 != bpf_probe_read(
            length,
            sizeof(*length),
            (void *)(slice + 0x8)))
    {
        return -1;
    }

    // Read bytes pointer.
    if (0 != bpf_probe_read(
            bytes,
            sizeof(*bytes),
            (void *)(slice + 0x10)))
    {
        return -1;
    }

    return 0;
}

/*
 * @brief   Fire perf events towards user-mode, one for each header.
 *
 * @param   metedata        The metadata to fire perf events for.
 *
 * @remark  The user should have initiated the given headers to zeros before
 *          filling them, so the data is exact.
 *
 * @return  -1 on failure.
 *          0 on success.
 */
static inline u32 fire_metadata_events(
    struct grpc_c_metadata_t * metadata,
    struct conn_id_t conn_id,
    uint32_t stream_id,
    uint64_t timestamp,
    int32_t stack_id,
    uint32_t direction,
    struct pt_regs * ctx)
{
    struct grpc_c_header_event_data_t * header_event = initiate_empty_grpc_header_event_data();
    if (NULL == header_event)
    {
        return -1;
    }
    header_event->conn_id = conn_id;
    header_event->stream_id = stream_id;
    header_event->timestamp = timestamp;
    header_event->stack_id = stack_id;
    header_event->direction = direction;

#pragma unroll
    for (uint32_t i = 0 ; i < MAXIMUM_AMOUNT_OF_ITEMS_IN_METADATA ; i++)
    {
        if (i >= metadata->count)
        {
            break;
        }

        #pragma unroll
        for (u32 j = 0 ; j < MAXIMUM_LENGTH_OF_KEY_IN_METADATA ; j++)
        {
            header_event->header.key[j] = metadata->items[i].key[j];
        }
        #pragma unroll
        for (u32 j = 0 ; j < MAXIMUM_LENGTH_OF_VALUE_IN_METADATA ; j++)
        {
            header_event->header.value[j] = metadata->items[i].value[j];
        }

        // Submit the current event.
        grpc_c_header_events.perf_submit(
            ctx,
            header_event,
            sizeof(*header_event));
    }

    return 0;
}

/*
 * @brief   Get the pointer to the flow controlled buffer of the stream.
 *          This buffer is a grpc_slice_buffer and contains data that is
 *          to be sent.
 *
 * @param   stream          The stream of which the buffer should be found.
 * @param   flow_controlled_buffer
 *                          The buffer to be found.
 * @param   version         The version of the grpc-c library being traced.
 *
 * @return  -1 on failure.
 *          0 on success.
 */
static inline u32 get_flow_controlled_buffer_from_stream(
    grpc_chttp2_stream * stream,
    /* OUT */ grpc_slice_buffer ** flow_controlled_buffer,
    const u64 version)
{
    if (NULL == stream || NULL == flow_controlled_buffer)
    {
        return -1;
    }

    switch (version)
    {
        case GRPC_C_V1_19_0:
            *flow_controlled_buffer = stream + 0x6e8; // Found with IDA, after "sending trailing_metadata" string.
            break;
        case GRPC_C_V1_24_1:
            *flow_controlled_buffer = stream + 0x980;
            break;
        case GRPC_C_V1_33_2:
            *flow_controlled_buffer = stream + 0x970;
            break;
        case GRPC_C_V1_41_1:
            *flow_controlled_buffer = stream + 0x978;
            break;
        default:
            return -1;
    }
    return 0;
}

/*
 * @brief   Get the slices of data inside a grpc_slice_buffer.
 *
 * @param   slice_buffer    The buffer of slices whose data should be extracted.
 * @param   write_event_data
 *                          The event data that will be used to fire perf events.
 *                          For each slice, the data will be put here and fired
 *                          to user-mode.
 * @param   connection_info
 *                          Data that relates to the HTTP2 connection on which the
 *                          slices are sent. This is used to determine and update
 *                          the position of the slice in the stream.
 *
 * @remark  The same struct is used for multiple perf events. This seems to work
 *          fine, but we should keep an eye out for erros caused by this.
 * @remark  The slice fired to user-mode is not "cleaned" (set to zeros) between
 *          events. The user should use the slice length to determine the size,
 *          because later indices may contain data from previous events.
 *
 * @return  -1 on failure.
 *          0 on success.
 */
static inline u32 get_slices_from_grpc_slice_buffer_and_fire_perf_event_per_slice(
    grpc_slice_buffer * slice_buffer,
    struct grpc_c_event_data_t * write_event_data,
    struct conn_info_t * connection_info,
    struct pt_regs * ctx)
{
    struct grpc_c_data_slice_t * data_slice = NULL;
    grpc_slice * slice = NULL;
    u32 slice_length = 0;
    void * slice_bytes = NULL;
    u32 length_to_read = 0;
    uint32_t amount_of_slices = 0;
    uint32_t event_data_length = 0;

    if (NULL == slice_buffer || NULL == write_event_data || NULL == connection_info || NULL == ctx)
    {
        return -1;
    }

    // Read amount of slices.
    if (0 != bpf_probe_read(
            &amount_of_slices,
            sizeof(amount_of_slices),
            (void *)(slice_buffer + 0x10)))
    {
        return -1;
    }

    // Read pointer to first slice.
    if (0 != bpf_probe_read(
            &slice,
            sizeof(slice),
            (void *)(slice_buffer + 0x08)))
    {
        return -1;
    }

    // If there are too many slices, only read the amount we're allowed to.
#pragma unroll
    for (u32 i = 0 ; i < SIZE_OF_DATA_SLICE_ARRAY ; i++)
    {
        if (i >= amount_of_slices)
        {
            break;
        }

        if (0 != get_data_ptr_from_slice(slice, &slice_length, &slice_bytes))
        {
            return -1;
        }
        if (NULL == slice_bytes)
        {
            return -1;
        }

        // Set the bytes and length to the slice.
        write_event_data->slice.slice_len = slice_length;
        length_to_read = slice_length;
        if (length_to_read > GRPC_C_SLICE_SIZE)
        {
            length_to_read = GRPC_C_SLICE_SIZE;
        }
        if (0 != bpf_probe_read(
                write_event_data->slice.bytes,
                length_to_read,
                (void *)(slice_bytes)))
        {
            return -1;
        }

        // Fill the position of the data slice.
        // We fill the absolute position, even if we did not copy all the data because the data
        // was too long.
        write_event_data->position_in_stream = connection_info->app_wr_bytes;
        connection_info->app_wr_bytes += write_event_data->slice.slice_len;

        // Submit the current event.
        event_data_length = sizeof(struct grpc_c_event_data_t) - GRPC_C_SLICE_SIZE + length_to_read;
        if (event_data_length == 0 || event_data_length >= sizeof(struct grpc_c_event_data_t))
        {
            // Can't really happen.
            return -1;
        }
        uint32_t event_data_length_minus_1 = event_data_length - 1;
        asm volatile("" : "+r"(event_data_length_minus_1) :);
        event_data_length = event_data_length_minus_1 + 1;
        grpc_c_events.perf_submit(
            ctx,
            write_event_data,
            event_data_length);

        // Reset the data that is specific per slice.
        write_event_data->position_in_stream = 0;
        write_event_data->slice.slice_len = 0;

        // Advance the slice pointer for the next iteration.
        slice += GRPC_SLICE_SIZE;
    }

    return 0;
}

/*
 * @brief   Read a metadata element list into a metadata struct, which we can
 *          pass to user-mode.
 * @detailed
 *          The list is a linked-list, and has a count of members in offset +0x00.
 *          The "head" is in offset +0x10 and the "tail" is in offset +0x18.
 *          We only use the head and read the list until its end.
 *          Each element is of type grpc_linked_mdelem which has an inlined
 *          grpc_mdelem object at its beginning, and then a pointer to the next
 *          grpc_linked_mdelem at offset +0x08.
 *          The grpc_mdelem is a single pointer which has its 2 least significant bits
 *          used for some storage thing which we don't care about.
 *          Once we ignore these bits, we are left with a pointer to a mdelem_data
 *          struct, which is simply 2 inlined grpc_slices one by one (the first is
 *          the key of the header, and the second is the value).
 *
 * @param   mdelem_list     The metadata element list to read.
 * @param   metadata        The metadata struct to be filled.
 *
 * @remark  The implementation of the metadata in grpc-c newer versions is vastly changed
 *          (they now use a newer map mechanism). For newer versions, this function will
 *          need an adjustment.
 *
 * @return  0 on success.
 *          Otherwise on failure.
 */
static inline int fill_metadata_from_mdelem_list(
    grpc_mdelem_list * mdelem_list,
    /* OUT */ struct grpc_c_metadata_t * metadata)
{
    grpc_linked_mdelem * current_linked_mdelem = NULL;
    void * grpc_mdelem_data_with_storage_bits = NULL;
    grpc_mdelem_data * mdelem_data = NULL;
    u32 current_length = 0;
    u32 to_copy = 0;
    void * current_bytes = NULL;

    if (NULL == mdelem_list)
    {
        // No metadata - this is fine, metadata is optional.
        return 0;
    }

    if (NULL == metadata)
    {
        return -1;
    }

    if (0 != bpf_probe_read(
            &metadata->count,
            sizeof(metadata->count),
            (void *)(mdelem_list)))
    {
        return -1;
    }

    if (0 != bpf_probe_read(
            &current_linked_mdelem,
            sizeof(current_linked_mdelem),
            (void *)(mdelem_list + 0x10)))
    {
        return -1;
    }

    for (u32 i = 0 ; i < MAXIMUM_AMOUNT_OF_ITEMS_IN_METADATA ; i++)
    {
        if (i >= metadata->count)
        {
            break;
        }
        if (NULL == current_linked_mdelem)
        {
            return -1;
        }

        // Get the mdelem info.
        if (0 != bpf_probe_read(
                &grpc_mdelem_data_with_storage_bits,
                sizeof(grpc_mdelem_data_with_storage_bits),
                (void *)(current_linked_mdelem)))
        {
            return -1;
        }
        if (NULL == grpc_mdelem_data_with_storage_bits)
        {
            return -1;
        }
        mdelem_data = (grpc_mdelem_data *)(((u64)grpc_mdelem_data_with_storage_bits >> 2) << 2);

        // Get the key.
        if (0 != get_data_ptr_from_slice(
                (grpc_slice *)mdelem_data,
                &current_length,
                &current_bytes))
        {
            return -1;
        }

        to_copy = current_length;
        if (to_copy > MAXIMUM_LENGTH_OF_KEY_IN_METADATA)
        {
            to_copy = MAXIMUM_LENGTH_OF_KEY_IN_METADATA;
        }
        if (0 != bpf_probe_read(
                metadata->items[i].key,
                to_copy,
                current_bytes))
        {
            return -1;
        }

        // Get the value.
        if (0 != get_data_ptr_from_slice(
                (grpc_slice *)(mdelem_data + GRPC_SLICE_SIZE),
                &current_length,
                &current_bytes))
        {
            return -1;
        }
        if (NULL == current_bytes)
        {
            return -1;
        }

        to_copy = current_length;
        if (to_copy > MAXIMUM_LENGTH_OF_VALUE_IN_METADATA)
        {
            to_copy = MAXIMUM_LENGTH_OF_VALUE_IN_METADATA;
        }
        if (0 != bpf_probe_read(
                metadata->items[i].value,
                to_copy,
                current_bytes))
        {
            return -1;
        }

        // Go forward in the linked list of mdelems.
        if (0 != bpf_probe_read(
                &current_linked_mdelem,
                sizeof(current_linked_mdelem),
                (void *)(current_linked_mdelem + 0x8)))
        {
            return -1;
        }
    }

    return 0;
}

/*
 * @brief   Handle metadata being fully received.
 * @detailed
 *          This is the immediate handler to the
 *          "grpc_chttp2_maybe_complete_recv_initial_metadata" and
 *          "grpc_chttp2_maybe_complete_recv_trailing_metadata" functions.
 *          This fires a perf buffer event for data being read, without actual data
 *          (only headers).
 *
 * @param   ctx             The context of the probe.
 * @param   is_initial      Whether handling initial metadata being received or trailing
 *                          metadata being received.
 *
 * @return  0 on success.
 *          Otherwise on failure.
 */
static inline int handle_maybe_complete_recv_metadata(struct pt_regs * ctx, const bool is_initial)
{
    struct grpc_c_event_data_t * read_data = initiate_empty_grpc_event_data();
    if (NULL == read_data)
    {
        return -1;
    }

    read_data->direction = GRPC_C_EVENT_DIRECTION_INCOMING;
    grpc_metadata_batch * metadata_batch = NULL;
    struct grpc_c_metadata_t * metadata = NULL;
    u32 key = 0;
    u32 initial_metadata_buffer_offset = 0;
    u32 trailing_metadata_buffer_offset = 0;
    u32 offset = 0;
    grpc_chttp2_stream * stream_ptr = NULL;
    grpc_chttp2_transport * transport_ptr = NULL;

    u32 pid = bpf_get_current_pid_tgid() >> 32;
    u32 fd = 0;
    u64 version = lookup_version(pid);
    if (GRPC_C_VERSION_UNSUPPORTED == version)
    {
        return 0;
    }
    read_data->timestamp = bpf_ktime_get_ns();
    transport_ptr = (grpc_chttp2_transport *)PT_REGS_PARM1(ctx);
    stream_ptr = (grpc_chttp2_stream *)PT_REGS_PARM2(ctx);
    read_data->stack_id = socket_stack_traces.get_stackid(
        ctx, BPF_F_USER_STACK | BPF_F_REUSE_STACKID);

    if (NULL == transport_ptr || NULL == stream_ptr)
    {
        return -1;
    }

    if (0 != get_stream_id((grpc_chttp2_stream *)stream_ptr, &read_data->stream_id, version))
    {
        return -1;
    }

    if (0 != get_fd_from_transport((grpc_chttp2_transport *)transport_ptr, &fd))
    {
        return -1;
    }

    struct conn_info_t* conn_info = get_or_create_conn_info(pid, fd);
    if (NULL == conn_info)
    {
        return -1;
    }
    read_data->conn_id = conn_info->conn_id;

    switch (version)
    {
        case GRPC_C_V1_19_0:
            initial_metadata_buffer_offset = 0x1e0;
            trailing_metadata_buffer_offset = 0x2d8;
            break;
        case GRPC_C_V1_24_1:
            // Not supported yet.
            break;
        case GRPC_C_V1_33_2:
            initial_metadata_buffer_offset = 0x330;
            trailing_metadata_buffer_offset = 0x570;
            break;
        case GRPC_C_V1_41_1:
            // Not supported yet.
            break;
        default:
            return -1;
    }

    if (is_initial)
    {
        offset = initial_metadata_buffer_offset;
    }
    else
    {
        offset = trailing_metadata_buffer_offset;
    }

    if (0 == offset)
    {
        // Offset unknown for this version.
        return 0;
    }

    metadata_batch = (grpc_chttp2_stream *)stream_ptr + offset;
    if (NULL == metadata_batch)
    {
        return 0;
    }

    metadata = initiate_empty_grpc_metadata();
    if (NULL == metadata)
    {
        return -1;
    }

    if (0 != fill_metadata_from_mdelem_list((grpc_mdelem_list *)metadata_batch, metadata))
    {
        return -1;
    }

    if (0 != fire_metadata_events(
        metadata,
        read_data->conn_id,
        read_data->stream_id,
        read_data->timestamp,
        read_data->stack_id,
        read_data->direction,
        ctx))
    {
        return -1;
    }

    return 0;
}

/*
 * @brief   Handle data being received.
 * @detailed
 *          This is the immediate handler to the
 *          "grpc_chttp2_data_parser_parse".
 *          It is called once for every slice being received.
 *          This function fires a perf buffer event for data being read.
 *          The event also contains optional headers.
 *
 * @param   ctx             The context of the probe.
 *
 * @return  0 on success.
 *          Otherwise on failure.
 */
int probe_grpc_chttp2_data_parser_parse(struct pt_regs *ctx)
{
    struct grpc_c_event_data_t * read_data = initiate_empty_grpc_event_data();
    if (NULL == read_data)
    {
        return -1;
    }

    read_data->direction = GRPC_C_EVENT_DIRECTION_INCOMING;
    grpc_slice * slice = NULL;
    u32 slice_length = 0;
    void * slice_bytes = NULL;
    grpc_metadata_batch * initial_metadata = NULL;
    grpc_metadata_batch * trailing_metadata = NULL;
    struct grpc_c_metadata_t * metadata = NULL;
    u32 key = 0;
    grpc_chttp2_stream * stream_ptr = NULL;
    grpc_chttp2_transport * transport_ptr = NULL;

    u32 pid = bpf_get_current_pid_tgid() >> 32;
    u32 fd = 0;
    u64 version = lookup_version(pid);
    if (GRPC_C_VERSION_UNSUPPORTED == version)
    {
        return 0;
    }
    read_data->timestamp = bpf_ktime_get_ns();
    transport_ptr = (grpc_chttp2_transport *)PT_REGS_PARM2(ctx);
    stream_ptr = (grpc_chttp2_stream *)PT_REGS_PARM3(ctx);
    read_data->stack_id = socket_stack_traces.get_stackid(
        ctx, BPF_F_USER_STACK | BPF_F_REUSE_STACKID);

    if (NULL == transport_ptr || NULL == stream_ptr)
    {
        return -1;
    }

    switch (version)
    {
        case GRPC_C_V1_19_0:
            // In this version, the slice is a stack-argument (struct copied by-value).
            slice = (void*)(ctx->sp + 0x08);
            break;
        case GRPC_C_V1_24_1:
        case GRPC_C_V1_33_2:
        case GRPC_C_V1_41_1:
            slice = (void*)PT_REGS_PARM4(ctx);
            break;
        default:
            return -1;
    }

    if (0 != get_stream_id((grpc_chttp2_stream *)stream_ptr, &read_data->stream_id, version))
    {
        return -1;
    }

    if (0 != get_fd_from_transport((grpc_chttp2_transport *)transport_ptr, &fd))
    {
        return -1;
    }

    if (0 != get_data_ptr_from_slice(slice, &slice_length, &slice_bytes))
    {
        return -1;
    }

    // Get the connection info.
    struct conn_info_t* conn_info = get_or_create_conn_info(pid, fd);
    if (NULL == conn_info)
    {
        return -1;
    }
    read_data->conn_id = conn_info->conn_id;

    // Get the headers. They're optional and can be a null pointer.
    if (0 != get_recv_initial_metadata_batch_from_stream(
            (grpc_chttp2_stream *)stream_ptr,
            &initial_metadata,
            version))
    {
        return -1;
    }
    if (NULL != initial_metadata)
    {
        metadata = initiate_empty_grpc_metadata();
        if (NULL == metadata)
        {
            return -1;
        }

        if (0 != fill_metadata_from_mdelem_list((grpc_mdelem_list *)initial_metadata, metadata))
        {
            return -1;
        }

        if (0 != fire_metadata_events(
            metadata,
            read_data->conn_id,
            read_data->stream_id,
            read_data->timestamp,
            read_data->stack_id,
            read_data->direction,
            ctx))
        {
            return -1;
        }
    }

    // Get the trailing headers (trailers). They're optional and can be a null pointer.
    if (0 != get_recv_trailing_metadata_batch_from_stream(
            (grpc_chttp2_stream *)stream_ptr,
            &trailing_metadata,
            version))
    {
        return -1;
    }
    if (NULL != trailing_metadata)
    {
        metadata = initiate_empty_grpc_metadata();
        if (NULL == metadata)
        {
            return -1;
        }

        if (0 != fill_metadata_from_mdelem_list((grpc_mdelem_list *)trailing_metadata, metadata))
        {
            return -1;
        }

        if (0 != fire_metadata_events(
            metadata,
            read_data->conn_id,
            read_data->stream_id,
            read_data->timestamp,
            read_data->stack_id,
            read_data->direction,
            ctx))
        {
            return -1;
        }
    }

    // Get the data.
    read_data->slice.slice_len = slice_length;
    u32 length_to_read = slice_length;
    if (length_to_read > GRPC_C_SLICE_SIZE)
    {
        length_to_read = GRPC_C_SLICE_SIZE;
    }
    if (0 != bpf_probe_read(
            read_data->slice.bytes,
            length_to_read,
            (void *)(slice_bytes)))
    {
        return -1;
    }

    // Fill the position of the data slice.
    // We fill the absolute position, even if we did not copy all the data because the data
    // was too long.
    read_data->position_in_stream = conn_info->app_rd_bytes;
    conn_info->app_rd_bytes += read_data->slice.slice_len;

    // Submit the event.
    // Trim the unneeded bytes from the tail, so that the perf ring buffer isn't filled up.
    // If the ring buffer is filled up, we'll start experiencing event losses.
    grpc_c_events.perf_submit(
        ctx,
        read_data,
        sizeof(struct grpc_c_event_data_t) - GRPC_C_SLICE_SIZE + length_to_read);

    return 0;
}

/*
 * @brief   Handle data being almost sent.
 * @detailed
 *          This is the immediate handler to the
 *          "grpc_chttp2_list_pop_writable_stream" function.
 *          It is called once per stream when the library checks if the stream
 *          has data to be sent. The handling of this function is at its return,
 *          and this entry probe only stores data for the return probe to use.
 *
 * @param   ctx             The context of the probe.
 *
 * @return  0 on success.
 *          Otherwise on failure.
 */
int probe_entry_grpc_chttp2_list_pop_writable_stream(struct pt_regs *ctx)
{
    struct list_pop_writable_stream_arguments args = { 0 };
    args.transport = (void*) PT_REGS_PARM1(ctx);
    args.stream = (void**) PT_REGS_PARM2(ctx);
    u32 zero = 0;
    list_pop_writable_stream_arguments_local.update(&zero, &args);
    return 0;
}

/*
 * @brief   Handle data being almost sent.
 * @detailed
 *          This is the immediate handler to the
 *          "grpc_chttp2_list_pop_writable_stream" function finishing.
 *          The library uses this function to iterate its list of writable streams.
 *          One of the arguments to this function is an out parameter - the stream.
 *          At the end on the function (which is why we use a return probe), we can
 *          see the stream being returned (if the function succeeded).
 *          The arguments to the function are stored earlier in the function's
 *          entry probe.
 *          When this is called for a stream, the stream may have multiple data slices
 *          ready to be sent. They are stored in the stream's flow_controlled_buffer.
 *          The probe ends with firing multiple perf buffer events.
 *              - The first event will have headers, if they are being sent.
 *              - There is one event per slice of data.
 *
 * @param   ctx             The context of the probe.
 *
 * @remark  For earlier versions, it seems that there's another buffer of data ready
 *          to be sent (not only flow_controlled_buffer, but also another buffer for
 *          compressed data). The data in the second buffer is not supported.
 *
 * @return  0 on success.
 *          Otherwise on failure.
 */
int probe_ret_grpc_chttp2_list_pop_writable_stream(struct pt_regs *ctx)
{
    struct grpc_c_event_data_t * write_data = initiate_empty_grpc_event_data();
    if (NULL == write_data)
    {
        return -1;
    }

    write_data->direction = GRPC_C_EVENT_DIRECTION_OUTGOING;
    int return_value = 0;
    u32 key = 0;
    u32 pid = bpf_get_current_pid_tgid() >> 32;
    u32 fd = 0;
    grpc_metadata_batch * initial_metadata = NULL;
    grpc_metadata_batch * trailing_metadata = NULL;
    struct grpc_c_metadata_t * metadata = NULL;
    write_data->timestamp = bpf_ktime_get_ns();
    write_data->stack_id = socket_stack_traces.get_stackid(
        ctx, BPF_F_USER_STACK | BPF_F_REUSE_STACKID);
    grpc_chttp2_stream * stream_ptr = NULL;
    grpc_chttp2_transport * transport_ptr = NULL;

    u64 version = lookup_version(pid);
    if (GRPC_C_VERSION_UNSUPPORTED == version)
    {
        return 0;
    }

    return_value = PT_REGS_RC(ctx);
    if (!return_value)
    {
        // The stream is invalid (loop finished).
        return 0;
    }

    struct list_pop_writable_stream_arguments * args =
            list_pop_writable_stream_arguments_local.lookup(&key);
    if (NULL == args)
    {
        // Arguments were not captured in function entry.
        return -1;
    }

    transport_ptr = (grpc_chttp2_transport *)args->transport;
    if (0 != bpf_probe_read(
            &(stream_ptr),
            sizeof(stream_ptr),
            (void*)(args->stream)))
    {
        return -1;
    }

    if (NULL == transport_ptr || NULL == stream_ptr)
    {
        return -1;
    }

    if (0 != get_stream_id((grpc_chttp2_stream *)stream_ptr, &(write_data->stream_id), version))
    {
        return -1;
    }

    if (0 != get_fd_from_transport((grpc_chttp2_transport *)transport_ptr, &fd))
    {
        return -1;
    }

    // Get the connection info.
    struct conn_info_t* conn_info = get_or_create_conn_info(pid, fd);
    if (NULL == conn_info)
    {
        return -1;
    }
    write_data->conn_id = conn_info->conn_id;

    // Get the headers. They're optional and can be a null pointer.
    if (0 != get_send_initial_metadata_batch_from_stream(
            (grpc_chttp2_stream *)stream_ptr,
            &initial_metadata,
            version))
    {
        return -1;
    }
    if (NULL != initial_metadata)
    {
        metadata = initiate_empty_grpc_metadata();
        if (NULL == metadata)
        {
            return -1;
        }

        if (0 != fill_metadata_from_mdelem_list((grpc_mdelem_list *)initial_metadata, metadata))
        {
            return -1;
        }

        if (0 != fire_metadata_events(
            metadata,
            write_data->conn_id,
            write_data->stream_id,
            write_data->timestamp,
            write_data->stack_id,
            write_data->direction,
            ctx))
        {
            return -1;
        }
    }

    // Get the trailing headers (AKA trailers). They're optional and can be a null pointer.
    if (0 != get_send_trailing_metadata_batch_from_stream(
            (grpc_chttp2_stream *)stream_ptr,
            &trailing_metadata,
            version))
    {
        return -1;
    }
    if (NULL != trailing_metadata)
    {
        metadata = initiate_empty_grpc_metadata();
        if (NULL == metadata)
        {
            return -1;
        }

        if (0 != fill_metadata_from_mdelem_list((grpc_mdelem_list *)trailing_metadata, metadata))
        {
            return -1;
        }

        if (0 != fire_metadata_events(
            metadata,
            write_data->conn_id,
            write_data->stream_id,
            write_data->timestamp,
            write_data->stack_id,
            write_data->direction,
            ctx))
        {
            return -1;
        }
    }

    // Get the data.
    // This only works for uncompressed data or for newer versions (in 1.44 only the flow_controlled_buffer
    // exists, but in 1.41.1 there's another buffer - compressed_data_buffer).
    grpc_slice_buffer * flow_controlled_buffer = NULL;
    if (0 != get_flow_controlled_buffer_from_stream(
        (grpc_chttp2_stream *)stream_ptr,
        &flow_controlled_buffer,
        version))
    {
        return -1;
    }

    u32 total_write_data_length = 0;
    if (0 != get_slices_from_grpc_slice_buffer_and_fire_perf_event_per_slice(
        flow_controlled_buffer,
        write_data,
        conn_info,
        ctx))
    {
        return -1;
    }

    return 0;
}

int probe_grpc_chttp2_mark_stream_closed(struct pt_regs *ctx)
{
    struct grpc_c_stream_closed_data data = { 0 };

    u32 pid = bpf_get_current_pid_tgid() >> 32;
    u32 fd = 0;
    data.timestamp = bpf_ktime_get_ns();
    data.stack_id = socket_stack_traces.get_stackid(
        ctx, BPF_F_USER_STACK | BPF_F_REUSE_STACKID);
    grpc_chttp2_stream * stream_ptr = NULL;
    grpc_chttp2_transport * transport_ptr = NULL;

    u64 version = lookup_version(pid);
    if (GRPC_C_VERSION_UNSUPPORTED == version)
    {
        return 0;
    }

    transport_ptr = (grpc_chttp2_transport *)PT_REGS_PARM1(ctx);
    stream_ptr = (grpc_chttp2_stream *)PT_REGS_PARM2(ctx);
    if (NULL == transport_ptr || NULL == stream_ptr)
    {
        return -1;
    }

    if (0 != get_stream_id((grpc_chttp2_stream *)stream_ptr, &data.stream_id, version))
    {
        return -1;
    }
    if (0 != get_fd_from_transport((grpc_chttp2_transport *)transport_ptr, &fd))
    {
        return -1;
    }

    uint32_t close_reads = PT_REGS_PARM3(ctx); // Whether 'read' is being closed.
    uint32_t close_writes = PT_REGS_PARM4(ctx); // Whether 'write' is being closed.
    if (close_reads)
    {
        data.read_closed = 1;
    }
    if (close_writes)
    {
        data.write_closed = 1;
    }

    // Get the connection info.
    struct conn_info_t* conn_info = get_or_create_conn_info(pid, fd);
    if (NULL == conn_info)
    {
        return -1;
    }
    data.conn_id = conn_info->conn_id;

    // Submit event
    grpc_c_close_events.perf_submit(ctx, &data, sizeof(data));

    return 0;
}

int probe_grpc_chttp2_maybe_complete_recv_initial_metadata(struct pt_regs *ctx)
{
    return handle_maybe_complete_recv_metadata(ctx, true);
}

int probe_grpc_chttp2_maybe_complete_recv_trailing_metadata(struct pt_regs *ctx)
{
    return handle_maybe_complete_recv_metadata(ctx, false);
}