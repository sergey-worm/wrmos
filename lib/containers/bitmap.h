//##################################################################################################
//
//  bitmap.h - template container.
//
//##################################################################################################

#ifndef BITMAP_H
#define BITMAP_H

template <unsigned MAX_BITS>
class bitmap_t
{
	enum { Bits_per_word = 8 * sizeof(long) };

	unsigned long _words [MAX_BITS / Bits_per_word + 1];
	unsigned _capacity;
	unsigned _count;

public:

	bitmap_t() : _capacity(0), _count(0)
	{
		for (unsigned i=0; i<(sizeof(_words)/sizeof(_words[0])); ++i)
			_words[i] = 0;
	}

	inline int init(unsigned capacity)
	{
		if (capacity > MAX_BITS)
			return -1;
		_capacity = capacity;
		_count = 0;
		return 0;
	}

	inline unsigned capacity()  const { return _capacity;  }
	inline unsigned count() const { return _count; }

	inline int get(unsigned pos) const
	{
		if (pos >= _capacity)
			return -1;
		return (_words[pos / Bits_per_word] >> (pos % Bits_per_word)) & 1;
	}

	inline int set(unsigned pos)
	{
		if (pos >= _capacity)
			return -1;
		if (get(pos) == 1)
			return 0;
		_count++;
		_words[pos / Bits_per_word] |= 1 << (pos % Bits_per_word);
		return 0;
	}

	inline int clear(unsigned pos)
	{
		if (pos >= _capacity)
			return -1;
		if (get(pos) == 0)
			return 0;
		_count--;
		_words[pos / Bits_per_word] &= ~(1 << (pos % Bits_per_word));
		return 0;
	}

	// find bit=0, set 1, return bit pos
	inline int getfree()
	{
		// find not full-busy word
		unsigned words_count = _capacity / Bits_per_word;
		for (unsigned i=0; i<words_count; ++i)
		{
			long w = _words[i];
			if (_words[i] != (unsigned long)-1)
			{
				// find free bit
				for (unsigned j=0; j<Bits_per_word; ++j)
				{
					if (!(w & (1 << j)))
					{
						unsigned pos = i * Bits_per_word + j;
						set(pos);
						return pos;
					}
				}
			}
		}
		// find free bit in tail
		unsigned bits_count = _capacity % Bits_per_word;
		long last_word = _words[words_count];
		for (unsigned i=0; i<bits_count; ++i)
		{
			if (!(last_word & (1 << i)))
			{
				unsigned pos = words_count * Bits_per_word + i;
				set(pos);
				return pos;
			}
		}
		return -1;
	}

	inline int getbusy()
	{
		// find not full-free word
		unsigned words_count = _capacity / Bits_per_word;
		for (unsigned i=0; i<words_count; ++i)
		{
			long w = _words[i];
			if (_words[i] != 0)
			{
				// find busy bit
				for (unsigned j=0; j<Bits_per_word; ++j)
				{
					if (w & (1 << j))
					{
						unsigned pos = i * Bits_per_word + j;
						clear(pos);
						return pos;
					}
				}
			}
		}
		// find busy bit in tail
		unsigned bits_count = _capacity % Bits_per_word;
		long last_word = _words[words_count];
		for (unsigned i=0; i<bits_count; ++i)
		{
			if (last_word & (1 << i))
			{
				unsigned pos = words_count * Bits_per_word + i;
				clear(pos);
				return pos;
			}
		}
		return -1;
	}

	inline int getany(int val)
	{
		return val ? getbusy() : getfree();
	}
};

#endif // BITMAP_H
