#include "tag.h"

#include "common.h"

#include <cstring> // memcpy
#include <vector>


struct __attribute__ ((__packed__)) Flags_t
{
	union
	{
		struct
		{
			uint ReadOnly	:  1;
			uint Type		:  2;
			uint Undefined	: 26;
			uint IsHeader	:  1;
			uint NoFooter	:  1;
			uint HasHeader	:  1;
		};
		uint uCell;
	};
};

struct __attribute__ ((__packed__)) Header_t
{
	union
	{
		char	cId[8];
		uint	uId[2];
	};
	uint	Version;
	uint	Size;
	uint	Items;
	Flags_t	Flags;
	uint	Reserved[2];

	bool isValidHeader() const { return (isValidId() &&  Flags.HasHeader &&  Flags.IsHeader && Version >= 2000); }
	bool isValidFooter() const { return (isValidId() && !Flags.NoFooter  && !Flags.IsHeader); }

private:
	bool isValidId() const
	{
		return (uId[0] == FOUR_CC('A','P','E','T') &&
				uId[1] == FOUR_CC('A','G','E','X') &&
				Flags.Undefined == 0);
	}
};

struct __attribute__ ((__packed__)) Item
{
	uint	Size;
	Flags_t	Flags;
	char	Key[];
	//char	Null;
	//char	Value[];
};

// ====================================
class CAPE : public Tag::IAPE
{
public:
	CAPE(const uchar* f_data, size_t f_offset, size_t f_size): m_tag(f_size)
	{
		memcpy(&m_tag[0], f_data + f_offset, f_size);
	}
	CAPE() = delete;

	void serialize(std::vector<unsigned char>& f_outStream) final override
	{
		f_outStream.insert(f_outStream.end(), m_tag.begin(), m_tag.end());
	}

	size_t getSize() const final override { return m_tag.size(); }

private:
	std::vector<uchar> m_tag;
};

// ====================================
namespace Tag
{
	size_t IAPE::getSize(const unsigned char* f_data, size_t f_offset, size_t f_size)
	{
		// Negative size is a result of parsing a footer-only tag, and now
		// the function is called with an offset pointing to the beginning
		// of that tag, and the real tag size should be returned now
		static const auto maxTagSize = std::numeric_limits<decltype(Header_t::Size)>::max();
		if(f_size > maxTagSize)
		{
			auto& f = *reinterpret_cast<const Header_t*>(f_data + f_offset + -f_size);
			ASSERT(f.isValidFooter());
			return (-f_size + sizeof(f));
		}

		// Check header/footer (there might be no header)
		auto& h = *reinterpret_cast<const Header_t*>(f_data + f_offset);
		if(sizeof(h) > f_size)
			return 0;
		auto size = f_size - sizeof(h);

		auto offset = f_offset + sizeof(h);
		if(h.isValidHeader())
		{
			ASSERT(h.Size == sizeof(h));
		}
		else if(h.isValidFooter())
		{
			ASSERT(h.Version == 2000);
			offset -= h.Size;
			size += h.Size;
			ASSERT(offset < f_offset);
		}
		else
			return 0;

		// Parse items
		auto pData = f_data + offset;
		for(uint i = 0; i < h.Items; i++)
		{
			auto& ii = *reinterpret_cast<const Item*>(pData);
			if(size < sizeof(ii))
				return 0;
			size -= sizeof(ii);

			for(pData = reinterpret_cast<const uchar*>(ii.Key); *pData && size; ++pData, --size) {}
			if(!size)
				return 0;

			pData += 1/*NULL*/ + ii.Size;
			if(size < ii.Size)
				return 0;
			size -= ii.Size;
		}

		// Check footer
		auto& f = *reinterpret_cast<const Header_t*>(pData);
		if(offset < f_offset)
		{
			// Footer-only mode
			ASSERT(&h == &f);
			return (offset - f_offset);
		}
		else
		{
			if(size < sizeof(f) || !f.isValidFooter())
				return 0;
			size -= sizeof(f);
			ASSERT(f.Size == reinterpret_cast<const uchar*>(&f + 1) - reinterpret_cast<const uchar*>(&h + 1));
			ASSERT(h.Items == f.Items);

			return (reinterpret_cast<const uchar*>(&f + 1) - reinterpret_cast<const uchar*>(&h));
		}
	}

	std::shared_ptr<IAPE> IAPE::create(const unsigned char* f_data, size_t f_offset, size_t f_size)
	{
		return std::make_shared<CAPE>(f_data, f_offset, f_size);
	}
}

