
class Html {
	enum class Tag {
		none,
		tag,
		skip,
		address_header,
		address_header_href,
		address_header_href_value,
		address_body,
		address_footer,
		paragraph_header,
		paragraph_body,
		paragraph_footer,
	};

	Tag tag = Tag::none;

	HugeString address_body;
	HugeString address_value;
	HugeString paragraph_body;

	HugeString result;

	void process_address_footer(const char c0, const char c1, const char c2, const char c3) {
		if (c0 == '>') { tag = Tag::none; }
		else if (c0 == '\\') { }
		else if (c0 == '/') { }
		else if (c0 == 'a') { }
		//else { verify(false); }
	}

	void process_address_body(const char c0, const char c1, const char c2, const char c3) {
		if (c0 == '<') { result += address_body; tag = Tag::address_footer; }
		else { address_body += c0; }
	}

	void process_address_header_href_value(const char c0, const char c1, const char c2, const char c3) {
		if (c0 == '"') { result += address_value; tag = Tag::address_header; }
		else { address_value += c0; }
	}

	void process_address_header_href(const char c0, const char c1, const char c2, const char c3) {
		if (c0 == '=') {}
		else if (c0 == '"') { address_value.clear(); tag = Tag::address_header_href_value; }
	}

	void process_address_header(const char c0, const char c1, const char c2, const char c3) {
		if (c0 == '>') { result += " body="; address_body.clear(); tag = Tag::address_body; }
		else if (c0 == 'h' && c1 == 'r' && c2 == 'e' && c3 == 'f') { result += " link="; tag = Tag::address_header_href; }
	}

	void process_paragraph_footer(const char c0, const char c1, const char c2, const char c3) {
		if (c0 == '>') { tag = Tag::none; }
		else if (c0 == '\\') { }
		else if (c0 == '/') { }
		else if (c0 == 'p') { }
		//else { verify(false); }
	}

	void process_paragraph_body(const char c0, const char c1, const char c2, const char c3) {
		if (c0 == '<') { result += paragraph_body; tag = Tag::paragraph_footer; }
		else { paragraph_body += c0; }
	}

	void process_paragraph_header(const char c0, const char c1, const char c2, const char c3) {
		if (c0 == '>') { result += " body="; paragraph_body.clear(); tag = Tag::paragraph_body; }
	}

	void process_skip(const char c0, const char c1, const char c2, const char c3) {
		if (c0 == '>') { tag = Tag::none; }
	}

	void process_tag(const char c0, const char c1, const char c2, const char c3) {
		if (c0 == 'a') { result += "\n[address]"; tag = Tag::address_header; }
		else if (c0 == 'p') { result += "\n[paragraph]"; tag = Tag::paragraph_header; }
		else { tag = Tag::skip; }
	}

	void process_none(const char c0, const char c1, const char c2, const char c3) {
		if (c0 == '<') { tag = Tag::tag; }
	}

	void process_chars(const char c0, const char c1, const char c2, const char c3) {
		switch (tag) {
			case Tag::none: process_none(c0, c1, c2, c3); break;
			case Tag::tag: process_tag(c0, c1, c2, c3); break;
			case Tag::skip: process_skip(c0, c1, c2, c3); break;
			case Tag::address_header: process_address_header(c0, c1, c2, c3); break;
			case Tag::address_header_href: process_address_header_href(c0, c1, c2, c3); break;
			case Tag::address_header_href_value: process_address_header_href_value(c0, c1, c2, c3); break;
			case Tag::address_body: process_address_body(c0, c1, c2, c3); break;
			case Tag::address_footer: process_address_footer(c0, c1, c2, c3); break;
			case Tag::paragraph_header: process_paragraph_header(c0, c1, c2, c3); break;
			case Tag::paragraph_body: process_paragraph_body(c0, c1, c2, c3); break;
			case Tag::paragraph_footer: process_paragraph_footer(c0, c1, c2, c3); break;
		}
	}

public:
	HugeString process(const HugeString& text) {
		for (size_t i = 0; i < text.size(); ++i) {
			const auto c0 = i < text.size() - 0 ? text[i + 0] : '0';
			const auto c1 = i < text.size() - 1 ? text[i + 1] : '1';
			const auto c2 = i < text.size() - 2 ? text[i + 2] : '2';
			const auto c3 = i < text.size() - 3 ? text[i + 3] : '3';
			process_chars(c0, c1, c2, c3);
		}
		return result;
	}
};

