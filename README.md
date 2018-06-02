# link-layer-chat-room
a simple chat room build on link layer

## Requirement

```
1. ether_type = 0x0701
2. msg format:
  | username (12 byte) | payload_length (4 byte) |
  |                 payload                      |
3. username is 12 bytes (including null char)
4. msg will be broadcasted if strlen(user input) > 0
5. Ctrl + C: clear user input
6. Ctrl + D: quit program
```

## Usage

```
make
./main -i <interface> [-h] [-u <username>]
```
