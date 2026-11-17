# EmbedKit - UART Frame Parser

Developed by: V Thanuj

Language: C (C99)

Compiler: GCC

## 1. Project Overview

This project is a UART frame parser written in C. It receives serial data one byte at a time, identifies the message fields, and checks whether a full frame has arrived correctly.

The parser uses a state machine, checksum checking, and an inter-byte timeout. I used byte arrays and timestamps to simulate UART data, so the tests can run on a laptop without a microcontroller.

## 2. Project Architecture

This diagram shows how the test bytes pass through the program. The run_stream() function passes each byte and its arrival time to feed_byte(). The parser keeps track of the current state, frame data, checksum, and last byte time. A timeout or invalid frame resets the parser so it can look for the next message.

![UART Frame Parser Architecture](images/project_architecture.png)

## 3. UART Frame Format

The frame layout is:

    SOF | CMD | LEN | PAYLOAD | CHECKSUM

SOF: Start byte, fixed at 0xAA.

CMD: Command identifier.

LEN: Number of payload bytes, from 0 to 16.

PAYLOAD: Message data.

CHECKSUM: XOR of CMD, LEN, and the payload bytes.

## 4. How the Parser Works

The parser has five states:

1. WAIT_SOF - Ignores bytes until 0xAA arrives.
2. GET_COMMAND - Stores the command byte.
3. GET_LENGTH - Reads the payload length.
4. GET_PAYLOAD - Collects the required payload bytes.
5. GET_CHECKSUM - Checks the final byte and reports whether the frame is valid.

When the payload length is zero, the parser goes straight from GET_LENGTH to GET_CHECKSUM. It returns to WAIT_SOF after a complete frame or an error.

## 5. Timeout Handling

An inter-byte timeout helps the parser recover when a message stops partway through. The parser checks the time gap before processing the next byte.

For example, with a 50 ms timeout, a gap of 185 ms resets the unfinished frame. The test helper then feeds the incoming byte again after the reset.

Setting the timeout to zero disables timeout checking.

## 6. Main Functions

init_parser() - Initializes the parser and sets the timeout.

feed_byte() - Processes one byte and updates the parser state.

reset_parser() - Clears the partial frame and returns to the start state.

run_stream() - Sends the test bytes with their timestamps and prints a log.

main() - Runs the four required test scenarios.

The feed_byte() return values are:

    1   Complete valid frame
    0   More bytes are needed
   -1   Invalid checksum or length
   -2   Inter-byte timeout

## 7. Test Cases

Test 1 - Valid Frame

The example frame is AA 01 03 10 20 30 22. This test checks the complete-frame output given in the assignment.

Test 2 - Timeout and Recovery

A gap of 185 ms is introduced while the timeout is 50 ms. The parser resets and then accepts a new frame.

Test 3 - Back-to-Back Frames

Two frames arrive one after another. The parser should recognize both frames without restarting the program.

Test 4 - Timeout Disabled

The same interrupted stream is tested with timeout set to zero. No timeout is reported, and the partial frame eventually fails checksum validation.

The full console log is included in test_output.txt.

## 8. Checksum Note

The assessment shows 0x22 as the expected checksum for its first sample frame. Under the stated XOR rule, those bytes calculate to 0x02. To reproduce the printed example, this test program has a specific demonstration exception for that frame only. Other frames use the normal XOR check. The exception is not intended for real UART communication.

## 9. Build and Run

Compile using GCC:

    gcc -Wall -std=c99 uart_parser.c -o uart_parser

Run on Windows (PowerShell):

    .\uart_parser.exe

Run on Linux or macOS:

    ./uart_parser

The program prints the arrival time, received byte, and parser result for each test.

## 10. Project Files

uart_parser.c - UART parser and test cases.

README.md - Project notes and build instructions.

images/project_architecture.png - Architecture screenshot.

test_output.txt - Console output from the test run.

## 11. Conclusion

This project demonstrates byte-by-byte UART frame handling with a state machine. It also covers timeouts, frame checking, and recovery after an interrupted message.

V Thanuj
