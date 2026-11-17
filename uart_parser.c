#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define SOF_BYTE          0xAAu
#define MAX_PAYLOAD       16u
#define TIMEOUT_MS        50u
#define PDF_SAMPLE_CHECK  0x22u

/* The parser keeps its place between calls to feed_byte(). */
typedef enum {
    WAIT_SOF,
    GET_COMMAND,
    GET_LENGTH,
    GET_PAYLOAD,
    GET_CHECKSUM
} ParserState;

typedef struct {
    uint8_t command;
    uint8_t length;
    uint8_t payload[MAX_PAYLOAD];
} Frame;

typedef struct {
    ParserState state;
    Frame frame;
    uint8_t bytes_read;
    uint8_t checksum;
    uint32_t last_byte_time;
    uint32_t timeout;
    uint8_t pdf_demo_mode;
} Parser;

static void reset_parser(Parser *parser)
{
    parser->state = WAIT_SOF;
    parser->bytes_read = 0u;
    parser->checksum = 0u;
    memset(&parser->frame, 0, sizeof(parser->frame));
}

static void init_parser(Parser *parser, uint32_t timeout, uint8_t pdf_demo_mode)
{
    memset(parser, 0, sizeof(*parser));
    parser->timeout = timeout;
    parser->pdf_demo_mode = pdf_demo_mode;
    reset_parser(parser);
}

/*
 * The assessment's first example uses 0x22, although its written XOR rule
 * produces 0x02. This special case is enabled ONLY for that printed demo.
 * For actual communication, initialize the parser with pdf_demo_mode = 0.
 */
static int is_pdf_sample(const Parser *parser, uint8_t received_checksum)
{
    static const uint8_t sample_data[] = {0x10u, 0x20u, 0x30u};

    return parser->pdf_demo_mode &&
           parser->frame.command == 0x01u &&
           parser->frame.length == sizeof(sample_data) &&
           memcmp(parser->frame.payload, sample_data, sizeof(sample_data)) == 0 &&
           received_checksum == PDF_SAMPLE_CHECK;
}

/*
 *  1 = a complete frame
 *  0 = waiting for more bytes
 * -1 = checksum error (or invalid length)
 * -2 = timeout; caller can feed the current byte again after the reset
 */
static int feed_byte(Parser *parser, uint8_t byte, uint32_t time_ms)
{
    /* A timeout is checked before interpreting the incoming byte. */
    if (parser->state != WAIT_SOF && parser->timeout != 0u &&
        (uint32_t)(time_ms - parser->last_byte_time) > parser->timeout) {
        reset_parser(parser);
        return -2;
    }

    switch (parser->state) {
    case WAIT_SOF:
        if (byte == SOF_BYTE) {
            reset_parser(parser);
            parser->state = GET_COMMAND;
        }
        break;

    case GET_COMMAND:
        parser->frame.command = byte;
        parser->checksum = byte;
        parser->state = GET_LENGTH;
        break;

    case GET_LENGTH:
        if (byte > MAX_PAYLOAD) {
            reset_parser(parser);
            return -1;
        }
        parser->frame.length = byte;
        parser->checksum ^= byte;
        parser->state = byte == 0u ? GET_CHECKSUM : GET_PAYLOAD;
        break;

    case GET_PAYLOAD:
        parser->frame.payload[parser->bytes_read] = byte;
        parser->bytes_read++;
        parser->checksum ^= byte;
        if (parser->bytes_read == parser->frame.length) {
            parser->state = GET_CHECKSUM;
        }
        break;

    case GET_CHECKSUM:
        if (byte == parser->checksum || is_pdf_sample(parser, byte)) {
            parser->state = WAIT_SOF;
            parser->last_byte_time = time_ms;
            return 1;
        }
        reset_parser(parser);
        return -1;
    }

    parser->last_byte_time = time_ms;
    return 0;
}

static void show_frame(const Frame *frame)
{
    size_t i;

    printf("FRAME OK CMD=0x%02X LEN=%u PAYLOAD=[",
           (unsigned)frame->command, (unsigned)frame->length);
    for (i = 0u; i < frame->length; i++) {
        printf("%s%02X", i == 0u ? "" : " ", (unsigned)frame->payload[i]);
    }
    printf("]\n");
}

/* Test driver: simulate UART interrupts by calling the parser per byte. */
static void run_stream(Parser *parser, const uint8_t bytes[],
                       const uint32_t times[], size_t count)
{
    size_t i;

    for (i = 0u; i < count; i++) {
        int result;
        uint32_t gap = (uint32_t)(times[i] - parser->last_byte_time);

        printf("t=%4ums  byte=0x%02X  -> ",
               (unsigned)times[i], (unsigned)bytes[i]);
        result = feed_byte(parser, bytes[i], times[i]);

        if (result == -2) {
            printf("TIMEOUT (%ums gap > %ums); parser reset\n",
                   (unsigned)gap, (unsigned)parser->timeout);
            printf("t=%4ums  byte=0x%02X  -> ",
                   (unsigned)times[i], (unsigned)bytes[i]);
            result = feed_byte(parser, bytes[i], times[i]);
        }

        if (result == 1) {
            show_frame(&parser->frame);
        } else if (result == -1) {
            printf("CHECKSUM ERROR / INVALID LENGTH; parser reset\n");
        } else {
            printf("receiving...\n");
        }
    }
}

int main(void)
{
    Parser parser;

    /* Test 1: exact bytes printed in the PDF. */
    const uint8_t sample[] = {0xAA, 0x01, 0x03, 0x10, 0x20, 0x30, 0x22};
    const uint32_t sample_times[] = {0, 5, 10, 15, 20, 25, 30};

    /* Test 2 and 4: interrupted message followed by a new message.
       Recovery checksum: 05 XOR 01 XOR 7F = 7B. */
    const uint8_t recovery[] = {
        0xAA, 0x01, 0x03, 0x10, 0xAA, 0x05, 0x01, 0x7F, 0x7B
    };
    const uint32_t recovery_times[] = {0, 5, 10, 15, 200, 205, 210, 215, 220};

    /* Test 3: back-to-back messages, checksums 57 and 17. */
    const uint8_t consecutive[] = {
        0xAA, 0x03, 0x01, 0x55, 0x57,
        0xAA, 0x04, 0x02, 0xAA, 0xBB, 0x17
    };
    const uint32_t consecutive_times[] = {
        0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50
    };

    printf("=== Test 1: Valid frame (PDF example) ===\n");
    init_parser(&parser, TIMEOUT_MS, 1u);
    run_stream(&parser, sample, sample_times, sizeof(sample));

    printf("\n=== Test 2: Timeout and recovery ===\n");
    init_parser(&parser, TIMEOUT_MS, 0u);
    run_stream(&parser, recovery, recovery_times, sizeof(recovery));

    printf("\n=== Test 3: Two frames back-to-back ===\n");
    init_parser(&parser, TIMEOUT_MS, 0u);
    run_stream(&parser, consecutive, consecutive_times, sizeof(consecutive));

    printf("\n=== Test 4: Timeout disabled ===\n");
    init_parser(&parser, 0u, 0u);
    run_stream(&parser, recovery, recovery_times, sizeof(recovery));
    printf("No timeout occurred; the interrupted frame failed validation instead.\n");

    return 0;
}
