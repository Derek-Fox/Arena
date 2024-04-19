# Protocol:
## Commands:

### HELP
- **Description**: Get a list of all available commands.
- **Usage**: `HELP`
- **Notes**: 
    - Server will respond with `OK` followed by a list of all available commands.

### LOGIN
- **Description**: Login to the server with a given username.
- **Usage**: `LOGIN <username>`
- **Notes**: 
    - User must not already be logged in.
    - The username must be between 1 and 20 characters long.
    - The username must be alphanumeric.
    - The username must be unique and not already in use by another user. If the username is already in use, the server will respond with an error message.
    - Server will respond with `OK` upon successful login.

### BYE
- **Description**: Disconnect from the server.
- **Usage**: `BYE`
- **Notes**: 
    - User must be logged in.
    - Server will respond with `OK`.

### STAT
- **Description**: Get the current arena that the user is in.
- **Usage`: `STAT`
- **Notes**: 
    - User must be logged in.
    - Server will respond with `OK` followed by the number of the arena the user is currently in.

### LIST
- **Description**: List all users currently in the arena.
- **Usage**: `LIST`
- **Notes**: 
    - User must be logged in.
    - Server will respond with `OK` followed by a comma separated list of usernames.

### MOVETO
- **Description**: Move to a different arena.
- **Usage**: `MOVETO <arena_number>`
- **Notes**: 
    - User must be logged in.
    - Server will respond with `OK` and the number of the arena the user is now in.
    - All users in the previous arena will be notified that the user has left with a `NOTICE`.
    - All users in the new arena will be notified that the user has joined with a `NOTICE`.

### MSG
- **Description**: Send a message to a user in the current arena.
- **Usage**: `MSG <username> <message>`
- **Notes**: 
    - User must be logged in.
    - The target of the message must be in the same arena as the user.
    - Server will respond with `OK` if the message was sent successfully.
    - The user receiving the message will be notified with a `NOTICE`, followed by the sender's name and message.

### BROADCAST
- **Description**: Send a message to all users in the current arena.
- **Usage**: `BROADCAST <message>`
- **Notes**: 
    - User must be logged in.
    - Server will respond with `OK` if the message was sent successfully.
    - All users in the arena will be notified with a `NOTICE`, followed by the sender's name and message.
