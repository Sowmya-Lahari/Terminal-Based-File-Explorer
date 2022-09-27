# Terminal-Based-File-Explorer
This is a Linux based File explorer. This works in Two modes
1. Normal Mode
2. Command Mode

### 1. Normal mode
This mode is used to navigate the file system using specific keys. This is the default mode of the file explorer. Below are the functionalities
- Display the content of current directory.
- Navigate the displayed list of files and directories using up and down arrows.
- Scrolling is handles using k (scroll up) and l (scroll down) keys.
- Open Directory upon presseing Enter key and display its content.
- Open the file in vi editor upon pressing enter key.
- Go back when left arrow is pressed and forward when right arrow is pressed
- Go up one level when backspace is pressed
- Goto home when h key is pressed

### 2. Command mode
In this mode the user can type the allowed commandsto perform actions. The command bar is at the bottom of the terminal. To enter command mode the user has to press : key. Below are the functionalities supported by command mode
- Copy, Move files and directories
- Rename a file
- Create and delete a file and directory
- Goto a specific location
- Search a file and directory
- To go back to normal mode ESC key should be pressed


#### TODO
- Handle window resizing
- Update the terminal content when actions are performed in Command mode
