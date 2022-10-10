#pragma once

struct Colors {
	Color clear = Color::rgba(1, 22, 39, 255);
	Color mode_text = Color::rgba(255, 155, 155, 255);
	Color status_line = Color::rgba(1, 22, 39, 255);
	Color status_text = Color::rgba(155, 155, 155, 255);
	Color notification_line = Color::rgba(1, 22, 39, 255);
	Color notification_text = Color::rgba(64, 152, 179, 255);
	Color cursor = Color::rgba(255, 255, 0, 255);
	Color cursor_line = Color::rgba(65, 80, 29, 255);
	Color whitespace = Color::rgba(75, 100, 93, 255);
	Color text = Color::rgba(205, 226, 239, 255);
	Color text_cursor = Color::rgba(5, 5, 5, 255);
	Color line_number = Color::rgba(75, 100, 121, 255);
	Color diff_note = Color::rgba(255, 192, 0, 255);
	Color diff_add = Color::rgba(0, 192, 0, 255);
	Color diff_remove = Color::rgba(192, 0, 0, 255);
};

static const Colors colors() {
	static Colors colors;
	return colors;
}

