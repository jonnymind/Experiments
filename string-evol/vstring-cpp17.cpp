// Variant String in C++17 style

#include <string>
#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <functional>
#include <memory>

/**
   String with variable internal storage size.
   Behaves like a std::string _EXCEPT_ for offering accessors to its elements as lvalues.

   As the actual type of the string content is unkonwn from outside, we cannot return
   a reference to an element in an arbitrary position (unless decorating it as i.e.
   a variant with a reference to the owner object, in case its sizing needs to be
   changed, which would be exceptionally sub-optimal).

   An alternative could be enlarging the string to its maximum sizing before
   if you really need it - and you know what you're doing scenario, but it's
   too dangerous to offer it as a vanilla component of the class.

   For this reason, all the iterators are read-only.
*/
class VariantString
{
public:
    class StringConcept  {
    public:
        virtual ~StringConcept() = default;
        virtual size_t char_size() const = 0;
        virtual size_t size() const = 0;
        virtual void resize (size_t n) =0;
        virtual void reserve (size_t n) =0;
        virtual void clear() =0;
        virtual const char* c_str() const =0;
        // Follows std::string::at semantics (bounds checked)
        virtual uint32_t at( size_t pos ) const = 0;
        // Like the operator, but explicitly non-lvalue.
        virtual uint32_t get_at( size_t pos ) const = 0;
        virtual void set_at( size_t pos, uint32_t v ) = 0;
        virtual void push_back( uint32_t v ) = 0;
        virtual StringConcept* clone() const = 0;
    };

    template<typename BaseString>
    class StringModel: public StringConcept {
    public:
        StringModel(): m_base( std::make_unique<BaseString>()  ) {}
        StringModel(const StringModel& other): m_base{std::make_unique<BaseString>(*other.m_base)} {}
        StringModel(StringModel&& other) noexcept: m_base{std::move(other.m_base)} {}
        StringModel(const BaseString& source): m_base{std::make_unique<BaseString>(source)} {}
        StringModel(BaseString* base): m_base{base} {}
        virtual ~StringModel() = default;

        virtual size_t char_size() const { return sizeof(typename BaseString::value_type); }
        virtual size_t size() const { return m_base->size(); }
        virtual void resize (size_t n) { return m_base->resize(n); }
        virtual void reserve (size_t n) { return m_base->reserve(n); }
        virtual void clear() { return m_base->clear(); }
        virtual const char* c_str() const { return reinterpret_cast<const char *>(m_base->c_str()); }
        virtual StringConcept* clone() const { return new StringModel(*this); }
        virtual uint32_t at( size_t pos ) const { return static_cast<uint32_t>(m_base->at(pos)); }
        virtual uint32_t get_at( size_t pos ) const { return static_cast<uint32_t>(m_base->at( pos )); }
        virtual void set_at( size_t pos, uint32_t v ) { m_base->at(pos) = static_cast<typename BaseString::value_type>(v); }
        virtual void push_back(uint32_t v) { m_base->push_back(static_cast<typename BaseString::value_type>(v)); }

    private: 
       std::unique_ptr<BaseString> m_base;
    };

    // Lambda used as template parameters with decltype!
    static constexpr auto incrementor = [](size_t a, size_t b) -> size_t{ return a + b; };
    static constexpr auto decrementor = [](size_t a, size_t b) -> size_t{ return a - b; };

    template<class VStr, class incrF>
    class iterator {
    public:
        iterator(VStr& owner, incrF f, size_t pos=0) noexcept: m_owner(owner), m_pos(pos), m_incr(f) {}
        iterator(const iterator& other) noexcept: m_owner(other.m_owner), m_pos(other.m_pos), m_incr(other.m_incr){}
        iterator& operator++() { m_pos = m_incr(m_pos, 1); return *this; }
        iterator& operator--() { m_pos = m_incr(m_pos, 1); return *this; }
        uint32_t& operator*() { m_chr = m_owner.get_at(m_pos); return m_chr; }
        iterator operator+(int count) const { return iterator(m_owner, m_incr, m_incr(m_pos, count)); }
        iterator operator-(int count) const { return iterator(m_owner, m_incr, m_incr(m_pos, -count)); }
        iterator operator+=(int count) { m_pos = m_incr(m_pos, count); return *this; }
        iterator operator-=(int count) {m_pos = m_incr(m_pos, -count); return *this; }
        bool operator==(const iterator& other) const { return other.m_pos == m_pos && &other.m_owner == &m_owner; }
        bool operator<(const iterator& other) const { return other.m_pos < m_pos && &other.m_owner == &m_owner; }
        bool operator!=(const iterator& other) const {return ! (*this == other); }
    private:
        VStr& m_owner;
        size_t m_pos{0};
        mutable uint32_t m_chr{0};
        incrF m_incr;
    };

    static StringConcept* make_properly_fitted_string(size_t char_size)
    {
        switch ( char_size ) {
        case sizeof( uint32_t ) :
            return new StringModel<std::u32string>;
        case sizeof( uint16_t ) :
            return new StringModel<std::u16string>;
        case sizeof( char ) :
            return new StringModel<std::string>;
        }
        throw std::invalid_argument( "Unknown char size" );
    }

    void adopt_model(StringConcept* model) {
        model->resize( m_string->size() );
        for ( size_t pos = 0; pos < model->size(); ++pos ) {
            model->set_at( pos, m_string->get_at( pos ) );
        }
        m_string.reset(model);
    }

    void refit( size_t char_size ) {
        if ( m_string->char_size() < char_size ) {
            adopt_model(make_properly_fitted_string( char_size ));
        }
    }

    void refit_if_too_large( uint32_t char_value ) {
        StringConcept* model = 0;
        if ( char_value >= 0x10000U && m_string->char_size() < 4) {
            adopt_model(make_properly_fitted_string( 4 ));
        }
        else if(char_value >= 0x100U && m_string->char_size() < 2) {
            adopt_model(make_properly_fitted_string( 2 ));
        }
    }

    static void toUtf8( std::ostream& out, uint32_t value ) {
        if( value >= 0x10000) {
            out << static_cast<char>( 0xF0 | (0x7 & value >> 18))
                << static_cast<char>( 0x80 | (0x3F & value >> 12))
                << static_cast<char>( 0x80 | (0x3F & value >> 6))
                << static_cast<char>( 0x80 | (0x3F & value));
        }
        else if( value >= 0x800) {
            out << static_cast<char>( 0xE0 | (0xF & value >> 12))
                << static_cast<char>( 0x80 | (0x3F & value >> 6))
                << static_cast<char>( 0x80 | (0x3F & value));
        }
        else if( value >= 0x80) {
            out << static_cast<char>( 0xC0 | (0x1F & value >> 6))
                << static_cast<char>( 0x80 | (0x3F & value));
        }
        else {
            out << static_cast<char>(value);
        }
    }


    template<typename CharT> 
    void copy_from_chars_inner( CharT* seq ) {
        while ( *seq ) {
            m_string->push_back( *seq );
            ++seq;
        }
    }

    template<typename CharT>
    void copy_from_chars( CharT* seq ) {
        refit(sizeof( CharT ));
        copy_from_chars_inner( seq );
    }

    // specialised for chars, which always fit
    void copy_from_chars( char* seq ) {
        copy_from_chars_inner( seq );
    }

    friend std::ostream& operator<<(std::ostream& out, const VariantString& str);

    std::unique_ptr<StringConcept> m_string;
public:
    VariantString(): m_string{std::make_unique<StringModel<std::string>>()} {}
    VariantString(const VariantString& other): m_string{other.m_string->clone()} {}
    VariantString(VariantString&& other) noexcept: m_string{std::move(other.m_string)} {
        std::cout << "Move constructor called\n";
        other.m_string = 0;
    }
    VariantString(size_t prealloc, size_t char_size=1): 
        m_string{make_properly_fitted_string(char_size)} 
    {
        m_string->reserve(prealloc);
    }
    ~VariantString() = default;

    template<typename CharT>
    VariantString(const CharT* s):
        m_string{ std::make_unique<StringModel<std::basic_string<CharT>>>()}
    {
        copy_from_chars(s);
    }

    template<typename CharT>
    VariantString(const std::basic_string<CharT>& s):
        m_string{std::make_unique<StringModel<decltype(s)>>()}
    {
        *m_string = s;
    }

    // we offer the const interator only    
    using const_iterator = iterator<const VariantString, decltype(incrementor)>;
    using const_riterator = iterator<const VariantString, decltype(decrementor)>;

    const_iterator begin() const {return const_iterator(*this, incrementor);}
    const_iterator end() const {return const_iterator(*this, incrementor, size());}
    const_riterator rbegin() const {return const_riterator(*this, decrementor, size()-1);}
    const_riterator rend() const {return const_riterator(*this, decrementor, std::string::npos);}

    // be kind and forward npos
    enum {npos = std::string::npos};
   
    // Usual denizens of std::string 
    size_t size() const { return m_string->size(); }
    size_t char_size() const { return m_string->char_size(); }
    void resize( size_t n ) { m_string->resize( n ); }
    void reserve( size_t n ) { m_string->reserve( n ); }
    void clear() { return m_string->clear(); }
    const char* c_str() const { return m_string->c_str(); }

    VariantString& operator=(const char* s)
    {
        clear();
        // Keep current char sizing
        copy_from_chars( s );
        return *this;
    }

    VariantString& operator=( const VariantString& other ) {
        if(&other != this) {
            m_string.reset(other.m_string->clone());
        }
        return *this;
    }
    
    // chars are always fitting
    void set_at(size_t pos, char chr) { m_string->set_at( pos, static_cast<uint32_t>(chr) ); }

    void set_at( size_t pos, uint32_t chr ) {
        refit_if_too_large( chr );
        m_string->set_at( pos, static_cast<uint32_t>(chr) );
    }

    uint32_t get_at( size_t pos ) const { return m_string->get_at( pos ); }

    void push_back( char c ) {
        // chars are always fitting
        m_string->push_back( c ); 
    }

    void push_back(uint32_t chr) {
        refit_if_too_large( chr );
        m_string->push_back( chr );
    }

    /* We offer only the const l-value version. */
    const uint32_t operator[]( size_t pos ) const {
        return m_string->get_at( pos );
    }

    /* We offer only the const l-value version. */
    const uint32_t at( size_t pos ) const {
        return m_string->at( pos );
    }

    template<typename StringT>
    VariantString& operator+=(const StringT& other) { 
        for(auto chr: other) { push_back(chr); } 
        return *this;
    }
    VariantString& operator+=(const char* other) {
        while(*other) {
            push_back(*other);
            ++other;
        }
        return *this;
    }

    VariantString& operator+=(char chr) { push_back(chr); return *this; }
    VariantString& operator+=(uint32_t chr) { push_back(chr); return *this; }

    template<typename StringT>
    VariantString operator +(const StringT& other) {
        VariantString nstr(*this);
        for(auto chr: other) {
            nstr.push_back(chr);
        }
        return nstr;
    }

    VariantString operator +(const char* other) {
        VariantString nstr(*this);
        while(*other) {
            nstr.push_back(*other);
            ++other;
        }
        return nstr;
    }

    VariantString operator +(char other) { VariantString nstr(*this); nstr.push_back(other); return nstr;}
    VariantString operator +(uint32_t other) { VariantString nstr(*this); nstr.push_back(other); return nstr;}

    VariantString substr(size_t pos, size_t len=npos) const {
        if(pos > size()) throw std::invalid_argument("Initial position out of range");
        if(len == 0) return "";
        if(len > size()) len = size() - pos;

        VariantString nstr(len, m_string->char_size());
        const_iterator iter = begin() + static_cast<int>(pos);
        const_iterator iend = begin() + static_cast<int>(pos+len);
        for(; iter != iend; ++iter) {nstr.push_back(*iter);}
        return nstr;
    }
};

std::ostream& operator<<(std::ostream& out, const VariantString& str) {
    for(auto chr: str) {
        VariantString::toUtf8(out, chr);
    }
    return out;
}

   
void inspect_string(const VariantString& utf_str) {
    std::cout << "Values in \"" << utf_str 
                << "\" length: " << utf_str.size() 
                << "; char-size: " << utf_str.char_size() << "\n";
    std::ios state(nullptr);
    state.copyfmt(std::cout);
    VariantString empty;
    int count = -1;
    
    for(auto chr: utf_str) {
        if(++count) {
            std::cout << ((count % 8 == 0)? "\n" : ", ");
        }
        std::cout << "U+" << std::hex << std::setw(4) << chr << ": " << (empty + chr);
    }
    std::cout << "\n";
    std::cout.copyfmt(state);
}

VariantString trivial_pass(VariantString a)
{
    return a;
}

int main() {
    VariantString empty; // used for simpler output
    VariantString vs("Hello world!");
    std::cout << vs << '\n';

    vs = "Reassignment: Hello world again!";
    std::cout << vs << '\n';
    
    vs = "Self-sum    : Hello world again ";
    vs += "and again!";
    std::cout << vs << '\n';

    std::cout << VariantString("Outer-sum   : Sum ") + "of" + std::string(" strings.") << '\n';

    VariantString utf_str("Expansion: Hello ");
    utf_str += 0x4E16U; // Se-
    utf_str += 0x754CU; // -Kai
    utf_str += '!';
    inspect_string(utf_str);

    VariantString utf_str2("Mutation: Hello world!");
    utf_str2.set_at(16, 0x4E16U);
    utf_str2.set_at(17, 0x754CU);
    inspect_string(utf_str2);
    
    std::cout << "Pos access: " << empty + utf_str2[10] << ", "
              << empty + utf_str2[16] << ", "
              << empty + utf_str2[17] << '\n';
    utf_str2.resize(18);
    std::cout << utf_str2 << "<<< cut here\n";

    VariantString vmoved("Testing the move constructor");
    std::cout << trivial_pass(vmoved) << '\n';
}
