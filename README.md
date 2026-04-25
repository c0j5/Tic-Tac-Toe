## Project Description
This project implements a Tic Tac Toe game as a loadable Linux kernel module. The module creates a character device at /dev/tictactoe that accepts game commands through write operations and returns game state through read operations. The implementation handles game logic including move validation, win condition detection, and a computer opponent that makes random moves using the kernel's random number generator.

## How to Configure, Compile, and Install the Custom Kernel
1. Install build dependencies: sudo apt update && sudo apt install linux-headers-$(uname -r) build-essential
2. Compile the kernel module: make
3. Install the module: sudo insmod kernelgame.ko
4. Verify installation: ls -la /dev/tictactoe && lsmod | grep tictactoe

## How to Compile and Run the Proof-of-Concept Userspace Program
1. The device itself is the interface - no separate userspace program needed
2. Test basic functionality: echo "BOARD" | sudo tee /dev/tictactoe && sudo cat /dev/tictactoe
3. Start a game: echo "START X" | sudo tee /dev/tictactoe && sudo cat /dev/tictactoe
4. Make moves: echo "PLAY 2,2" | sudo tee /dev/tictactoe && sudo cat /dev/tictactoe

## How to Compile and Run the Testing Suite Userspace Program
1. Create a test script (test_commands.sh) with command sequences from project specification
2. Make executable: chmod +x test_commands.sh
3. Run test suite: ./test_commands.sh
4. Test module reload: for i in {1..3}; do sudo insmod kernelgame.ko; echo "BOARD" | sudo tee /dev/tictactoe; sudo rmmod kernelgame; done

## Known Project Issues
1. Game does not distinguish between win and tie conditions - both return "GAME_OVER"
2. No persistence of game state across device file opens/closes
3. Limited error messages for edge cases beyond specification requirements
4. Bot AI is completely random with no logic

## LLM/AI Prompts Used
1. Fix C90 compatibility issues and kernel compilation errors
2. Create comprehensive test sequences to verify all rubric requirements

## Sources Used
1. Google Drive Project 4 resource files
2. Project 4 PDF - Command formats and error codes
