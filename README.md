# BBB_Security_Service

## Project Overview

This project is a security system running on BeagleBone Black (BBB), integrating fingerprint recognition and member management through a SQLite database. The application runs on Linux, communicates with a fingerprint processing module / STM32F4 MCU via UART, and supports administrative operations through a command-line interface (CLI).

The main goals of the system are:

- Fingerprint recognition and member identity lookup
- Storing user information in the database
- Managing lock/unlock states when a user fails fingerprint verification too many times
- Recording event history in a log file
- Providing an admin interface to add/remove members

---

## System Architecture

The project consists of the following main components:

- `main.c`: program entry point, UART and SQLite initialization, and main thread startup
- `main.h`: constant definitions, FSM state enum, global variables, and function prototypes
- `uart.c`: UART configuration and management for `/dev/ttyS1`
- `database.c`: SQLite queries for looking up, adding, and deleting members
- `threads.c`: time handling, fingerprint processing, and admin CLI logic
- `Makefile`: cross-compilation for ARM and automatic binary upload to the BBB via SCP

### Main Processing Flow

1. The application initializes UART and SQLite.
2. It creates the following threads:
   - a thread that sends timestamps over UART for display on the standby screen
   - a thread that receives fingerprint-related data from the STM32F4
   - a thread that handles admin commands via CLI
3. When the STM32F4 sends a status and fingerprint ID, the program:
   - looks up the corresponding name in the database
   - sends the result back to the module
   - writes the event to the history log
4. If a new fingerprint is detected, the system waits for the `add <ID> <Name>` command to save the new user.
5. If the user fails fingerprint verification multiple times, the system enters lock states for 5 minutes, 10 minutes, or permanent lock depending on the situation.

---

## Main Features

### 1. Fingerprint Management

The system handles multiple FSM states of the fingerprint module, including:

- `FSM_FINGER_SEND_GENIMG`
- `FSM_FINGER_WAIT_GENIMG`
- `FSM_FINGER_SEND_IMG2TZ`
- `FSM_FINGER_WAIT_IMG2TZ`
- `FSM_FINGER_SEND_SEARCH`
- `FSM_FINGER_WAIT_SEARCH`
- `FSM_FINGER_DELAY`
- `FSM_FINGER_BLOCK_5M`
- `FSM_FINGER_BLOCK_10M`
- `FSM_FINGER_BLOCK_INF`
- `FSM_FINGER_UNBLOCK`
- `FSM_SYSTEM_STM32F4_WAKEUP`
- `FSM_NEW_FINGERPRINT_ADDED`
- `FSM_REMOVE_SPECIFIC_FINGERPRINT`

These states allow the system to coordinate the acquisition, matching, and lock/unlock management of the fingerprint module.

### 2. UART Communication

- Opens the `/dev/ttyS1` port
- Configures baudrate 115200, 8N1
- Sends system time using the format `#TS=<timestamp>;`
- Receives status frames from the module and responds with `#State=x,#Name=...;`

### 3. SQLite Database Management

- Default database: `memberdb.db`
- Default table: `memberlst`
- Functions include:
  - `get_user_name(id, output_name)`: retrieves a name by ID
  - `add_new_member(id, name)`: adds a new member
  - `delete_member(id)`: removes a member by ID

### 4. Multithreading Support

The application uses `pthread` to run multiple tasks concurrently:

- `handle_TimeDisplay_thread`: periodically sends timestamps over UART
- `handle_SearchMember_thread`: reads UART data, parses states, matches fingerprints, and queries the database
- `handle_CLI_thread`: handles admin commands from the terminal

### 5. CLI Administration

The system supports admin commands such as:

- `add <ID> <Name>`: adds a new member after the module detects a new fingerprint
- `delete <ID>`: removes a member from the database
- `exit`: can terminate the system if enabled in the current code version

Example:

```bash
BBB_Admin> add 2 Nguyen Van A
BBB_Admin> delete 5
```

### 6. History Logging

Important events are saved to:

- `Security_history_log.txt`

Events may include:

- recognized users
- unknown users
- lock events
- unlock events
- add/remove member actions

---

## Key Data Structures and Constants

Several important constants are defined in `main.h`:

- `MAX_NAME_LEN = 16`
- `LOG_BUFFER_SIZE = 128`
- `UART_DEVICE = "/dev/ttyS1"`
- `DATABASE_NAME = "memberdb.db"`
- `DATABASE_TABLE_NAME = "memberlst"`
- `NONE_FINGER_PRINT_ID = 99`
- `OFFSET_MEMBER_ID = 1`

This configuration indicates that the project works with fingerprint IDs offset from the database IDs, and uses a default ID value of 99 for an unset fingerprint.

---

## Environment Requirements

- Linux operating system
- BeagleBone Black or other compatible ARM-based device
- UART port `/dev/ttyS1`
- SQLite3 library
- ARM cross-compiler GCC (`arm-linux-gnueabihf-gcc`)

---

## Practical Application Use Cases

This project is suitable for fingerprint-based access control systems such as:

- office entry/exit control
- automatic door control
- attendance or personnel management systems
- security applications requiring user identity verification

---

## Internal Reference Files

The main files to review during further development are:

- `main.h` – shared state definitions and API declarations
- `database.c` – database operations
- `uart.c` – serial communication configuration
- `threads.c` – main business logic threads

---

## Conclusion

BBB_Security_Service is a fingerprint-based authentication and member management system running on BeagleBone Black, combining UART, SQLite, multithreading, and CLI control. It is a strong foundation for expanding into a complete real-world access control product with security features, event logging, and easy administration.
