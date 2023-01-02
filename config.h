#pragma once

struct Colors {
	Color clear = Color::rgba(32, 32, 18, 255);
	Color overlay = Color::rgba(31, 52, 69, 255);
	Color cursor = Color::rgba(255, 255, 0, 255);
	Color cursor_line = Color::rgba(65, 80, 29, 255);
	Color whitespace = Color::rgba(11, 32, 42, 255);
	Color text = Color::rgba(200, 200, 200, 255);
	Color text_cursor = Color::rgba(5, 5, 5, 255);
	Color long_line = Color::rgba(255, 0, 0, 255);
	Color line_number = Color::rgba(75, 100, 121, 255);
	Color highlight = Color::rgba(192, 0, 192, 255);
	Color diff_note = Color::rgba(255, 192, 0, 255);
	Color diff_add = Color::rgba(0, 192, 0, 255);
	Color diff_remove = Color::rgba(192, 0, 0, 255);
	Color keyword = Color::rgba(199, 146, 234, 255);
	Color clas = Color::rgba(180, 180, 180, 255);
	Color comment = Color::rgba(255, 255, 0, 255);
	Color punctuation = Color::rgba(127, 219, 202, 255);
	Color quote = Color::rgba(247, 140, 108, 255);
};

Colors& colors() {
	static Colors colors;
	return colors;
}


struct HSB {
	float hue_start = 0.0f;
	float hue_range = 260.0f;
	float hue_adjust = 10.0f;
	float saturation = 0.50f;
	float brightness = 0.85f;

	SmallString increase_hue_start() { hue_start = clamp(hue_start + 5.0f, 0.0f, 360.0f); return SmallString("hue_start = ") + SmallString(hue_start); }
	SmallString decrease_hue_start() { hue_start = clamp(hue_start - 5.0f, 0.0f, 360.0f); return SmallString("hue_start = ") + SmallString(hue_start); }

	SmallString increase_hue_range() { hue_range = clamp(hue_range + 5.0f, 0.0f, 360.0f); return SmallString("hue_range = ") + SmallString(hue_range); }
	SmallString decrease_hue_range() { hue_range = clamp(hue_range - 5.0f, 0.0f, 360.0f); return SmallString("hue_range = ") + SmallString(hue_range); }

	SmallString increase_hue_adjust() { hue_adjust = clamp(hue_adjust + 5.0f, 0.0f, 360.0f); return SmallString("hue_adjust = ") + SmallString(hue_adjust); }
	SmallString decrease_hue_adjust() { hue_adjust = clamp(hue_adjust - 5.0f, 0.0f, 360.0f); return SmallString("hue_adjust = ") + SmallString(hue_adjust); }

	SmallString increase_saturation() { saturation = clamp(saturation + 0.05f, 0.0f, 1.0f); return SmallString("saturation = ") + SmallString(saturation); }
	SmallString decrease_saturation() { saturation = clamp(saturation - 0.05f, 0.0f, 1.0f); return SmallString("saturation = ") + SmallString(saturation); }

	SmallString increase_brightness() { brightness = clamp(brightness + 0.05f, 0.0f, 1.0f); return SmallString("brightness = ") + SmallString(brightness); }
	SmallString decrease_brightness() { brightness = clamp(brightness - 0.05f, 0.0f, 1.0f); return SmallString("brightness = ") + SmallString(brightness); }
};

HSB& hsb() {
	static HSB hsb;
	return hsb;
}


struct Spacing {
	float character = 8.0f;
	float line = 20.0f;
	float zoom = 1.0f;

	SmallString increase_char_width() { character = clamp(character + 0.05f, 0.0f, 20.0f); return SmallString("char_width = ") + SmallString(character); }
	SmallString decrease_char_width() { character = clamp(character - 0.05f, 0.0f, 20.0f); return SmallString("char_width = ") + SmallString(character); }

	SmallString increase_char_height() { line = clamp(line + 0.05f, 0.0f, 30.0f); return SmallString("char_height = ") + SmallString(line); }
	SmallString decrease_char_height() { line = clamp(line - 0.05f, 0.0f, 30.0f); return SmallString("char_height = ") + SmallString(line); }

	SmallString increase_char_zoom() { zoom = clamp(zoom + 0.01f, 0.0f, 2.0f); return SmallString("char_zoom = ") + SmallString(zoom); }
	SmallString decrease_char_zoom() { zoom = clamp(zoom - 0.01f, 0.0f, 2.0f); return SmallString("char_zoom = ") + SmallString(zoom); }
};

Spacing& spacing() {
	static Spacing spacing;
	return spacing;
}

