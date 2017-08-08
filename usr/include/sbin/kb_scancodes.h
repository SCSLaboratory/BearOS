#pragma once
/* kb_scancodes.h
*
* Keyboards output 8-bit scancodes corresponding to the keys on the
*  keyboard.  Unfortunately, these codes correspond to the position
*  of the key on the keyboard, not to the ASCII code associated with
*  that letter.  Therefore, lookup tables are necessary to convert
*  from the scancode to something printable.
*
* List of scancodes referenced from
*  http://www.win.tue.nl/~aeb/linux/kbd/scancodes-1.html
*/

#define ESC_OFFSET 89

static char lowercase[89] = {
	'%', // error code
	'%', // Esc
	'1',
	'2',
	'3',
	'4',
	'5',
	'6',
	'7',
	'8',
	'9',
	'0',
	'-',
	'=',
	'%', // Backspace
	'\t',
	'q',
	'w',
	'e',
	'r',
	't',
	'y',
	'u',
	'i',
	'o',
	'p',
	'[',
	']',
	'\n', // Enter
	'%', // Left Control
	'a',
	's',
	'd',
	'f',
	'g',
	'h',
	'j',
	'k',
	'l',
	';',
	'\'',
	'`',
	'%', // Left Shift
	'\\',
	'z',
	'x',
	'c',
	'v',
	'b',
	'n',
	'm',
	',',
	'.',
	'/',
	'%', // Right Shift
	'*',
	'%', // Left Alt
	' ',
	'%', // Caps Lock
	'%', // F1 - F10
	'%',
	'%',
	'%',
	'%',
	'%',
	'%',
	'%',
	'%',
	'%',
	'%', // Numlock
	'%', // Scroll Lock
	'7', // Keypad
	'8',
	'9',
	'-',
	'4',
	'5',
	'6',
	'+',
	'1',
	'2',
	'3',
	'0',
	'.',
	'%', // Empty
	'%', // Empty
	'%', // Windows key
	'%', // F11
	'%', // F12
};

static char uppercase[89] = {
	'%', // error code
	'%', // Esc
	'!',
	'@',
	'#',
	'$',
	'%',
	'^',
	'&',
	'*',
	'(',
	')',
	'_',
	'+',
	'%', // Backspace
	'\t',
	'Q',
	'W',
	'E',
	'R',
	'T',
	'Y',
	'U',
	'I',
	'O',
	'P',
	'{',
	'}',
	'\n', // Enter
	'%', // Left Control
	'A',
	'S',
	'D',
	'F',
	'G',
	'H',
	'J',
	'K',
	'L',
	':',
	'"',
	'~',
	'%', // Left Shift
	'|',
	'Z',
	'X',
	'C',
	'V',
	'B',
	'N',
	'M',
	'<',
	'>',
	'?',
	'%', // Right Shift
	'*',
	'%', // Left Alt
	' ',
	'%', // Caps Lock
	'%', // F1 - F10
	'%',
	'%',
	'%',
	'%',
	'%',
	'%',
	'%',
	'%',
	'%',
	'%', // Numlock
	'%', // Scroll Lock
	'7', // Keypad
	'8',
	'9',
	'-',
	'4',
	'5',
	'6',
	'+',
	'1',
	'2',
	'3',
	'0',
	'.',
	'%', // Empty
	'%', // Empty
	'%', // Windows key
	'%', // F11
	'%', // F12
};
