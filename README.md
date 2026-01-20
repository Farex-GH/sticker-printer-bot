# Telegram Sticker Printer Bot

A sticker printer bot, intended for use with a Phomemo M02 Pro printer.

## Requirements

* Telegram API key
* Imagemagick
* bluetoothctl (USB connection may work too, but I don't actively test it)
* A C++ compiler that supports C++23
* Linux (a Raspberry Pi 4 has more than enough power to run the bot)
* Phomemo M02 Pro printer

## Setup

### Telegram

For creating the Telegram bot and getting a Telegram token, see https://core.telegram.org/bots/tutorial#obtain-your-bot-token.

### Building

```
git submodule init
```

Follow tgbot-cpp installation instructions: https://github.com/reo7sp/tgbot-cpp

```
make test_bot -j$(nproc)
```

## Running

I recommend pasting the below into a shell script for ease.

```
# Printer MAC can be found with bluetoothctl
PRINTER_MAC=""
# Telegram API token
TOKEN=""

bluetoothctl pair ${PRINTER_MAC}
sudo rfcomm bind 0 ${PRINTER_MAC}
./bot.elf ${TOKEN}
```

The Makefile contains some extra build options for testing or debugging.

## Quality

This code is hobbyist at best, is missing printer error codes, has plenty of TODOs, and would need some rework to work with other printers. However, as-is, it should be mostly stable.

The code also assumes this is running on Linux (hence why Linux is a requirement). WSL may work, but I've never used it.

## Known Issues

1. If the printer has an image it's trying to print while the paper is empty, it might print a long blank page after new paper is added.

2. Images aren't normalized. I don't remember why I decided not to normalize things, but there was a reason at one point, and it might be fine to normalize them again

3. Battery life reported is sometimes erratic. I think this happens if we retrieve battery life too soon after a print, however it's close enough to being accurate, and if it's off the consequences are negligible.
.
