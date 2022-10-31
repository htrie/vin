
struct FontGlyph {
	float x = 0.0f;
	float y = 0.0f;
	float w = 0.0f;
	float h = 0.0f;
	float x_off = 0.0f;
	float y_off = 0.0f;
	float x_adv = 0.0f;

	FontGlyph(float x, float y, float w, float h, float x_off, float y_off, float x_adv)
		: x(x), y(y), w(w), h(h), x_off(x_off), y_off(y_off), x_adv(x_adv) {}
};

