
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

	void process_address_footer(const char c) {
		if (c == '>') { tag = Tag::none; }
		else if (c == '\\') { }
		else if (c == '/') { }
		else if (c == 'a') { }
		//else { verify(false); }
	}

	void process_address_body(const char c) {
		if (c == '<') { result += address_body; tag = Tag::address_footer; }
		else { address_body += c; }
	}

	void process_address_header_href_value(const char c) {
		if (c == '"') { result += address_value; tag = Tag::address_header; }
		else { address_value += c; }
	}

	void process_address_header_href(const char c) {
		if (c == 'r') {}
		else if (c == 'e') {}
		else if (c == 'f') {}
		else if (c == '=') {}
		else if (c == '"') { address_value.clear(); tag = Tag::address_header_href_value; }
	}

	void process_address_header(const char c) {
		if (c == '>') { result += " body="; address_body.clear(); tag = Tag::address_body; }
		else if (c == 'h') { result += " link="; tag = Tag::address_header_href; }
	}

	void process_paragraph_footer(const char c) {
		if (c == '>') { tag = Tag::none; }
		else if (c == '\\') { }
		else if (c == '/') { }
		else if (c == 'p') { }
		//else { verify(false); }
	}

	void process_paragraph_body(const char c) {
		if (c == '<') { result += paragraph_body; tag = Tag::paragraph_footer; }
		else { paragraph_body += c; }
	}

	void process_paragraph_header(const char c) {
		if (c == '>') { result += " body="; paragraph_body.clear(); tag = Tag::paragraph_body; }
	}

	void process_skip(const char c) {
		if (c == '>') { tag = Tag::none; }
	}

	void process_tag(const char c) {
		if (c == 'a') { result += "\n[address]"; tag = Tag::address_header; }
		else if (c == 'p') { result += "\n[paragraph]"; tag = Tag::paragraph_header; }
		else { tag = Tag::skip; }
	}

	void process_none(const char c) {
		if (c == '<') { tag = Tag::tag; }
	}

public:
	HugeString process(const HugeString& text) {
		for (auto& c : text) {
			switch (tag) {
				case Tag::none: process_none(c); break;
				case Tag::tag: process_tag(c); break;
				case Tag::skip: process_skip(c); break;
				case Tag::address_header: process_address_header(c); break;
				case Tag::address_header_href: process_address_header_href(c); break;
				case Tag::address_header_href_value: process_address_header_href_value(c); break;
				case Tag::address_body: process_address_body(c); break;
				case Tag::address_footer: process_address_footer(c); break;
				case Tag::paragraph_header: process_paragraph_header(c); break;
				case Tag::paragraph_body: process_paragraph_body(c); break;
				case Tag::paragraph_footer: process_paragraph_footer(c); break;
			}
		}
		return result;
	}
};

