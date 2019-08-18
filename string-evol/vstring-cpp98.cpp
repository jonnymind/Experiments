// Variant String in C98 style

#include <string>
#include <stdexcept>

/* String with variable internal storage size.
   Behaves like a std::string _EXCEPT_ for offering accessors to its elements as lvalues.

   As the actual type of the string content is unkonwn from outside, we cannot return
   a reference to an element in an arbitrary position (unless decorating it as i.e.
   a variant with a reference to the owner object, in case its sizing needs to be
   changed, which would be exceptionally sub-optimal).

   An alternative could be enlarging the string to its maximum sizing before
   offering an element-lvalue return; that would work in a "IYRNI-YKWYAD"
   if you really need it - and you know what you're doing scenario, but it's
   too dangerous to offer it as a vanilla component of the class.

   For this reason, al the iterators are read-only.
*/
class VariantString
{
public:
    class StringConcept  {
    public:
        virtual size_t char_size() const = 0;
        virtual size_t size() const = 0;
        virtual void resize (size_t n) =0;
        virtual void clear() =0;
        virtual const char* c_str() const =0;
        /*Follows std::string::at semantics (bounds checked)*/
        virtual uint32_t at( size_t pos ) const = 0;
        /* Like the operator, but explicitly non-lvalue. */
        virtual uint32_t get_at( size_t pos ) const = 0;
        virtual void set_at( size_t pos, uint32_t v ) = 0;
        virtual void push_back( uint32_t v ) = 0;
    };

    template<typename BaseString=std::string>
    class StringModel: public StringConcept {
    public:
        StringModel():
            m_base( new BaseString )
        {}

        StringModel(const StringModel& other) :
            m_base( new BaseString(other->m_base) )
        {}

        StringModel( const BaseString& source ) :
            m_base( new BaseString( other ) )
        {}

        StringModel(BaseString* base): 
            m_base(base) 
        {}

        ~StringModel() { delete m_base; }


        virtual size_t char_size() const { return sizeof(BaseString::value_type); }
        virtual size_t size() const { return m_base->size(); }
        virtual void resize (size_t n) { return m_base->resize(n); }
        virtual void clear() { return m_base->clear(); }
        virtual const char* c_str() const { return m_base->c_str(); }
        virtual uint32_t at( size_t pos ) const {
            return static_cast<uint32_t>(m_base->at(pos));
        }

        virtual uint32_t get_at( size_t pos ) const {
            return static_cast<uint32_t>(m_base->at( pos ));
        }
        virtual void set_at( size_t pos, uint32_t v ) {
            m_base->at(pos) = static_cast<typename BaseString::value_type>(v);
        }

        virtual void push_back(uint32_t v) {
            m_base->push_back(static_cast<typename BaseString::value_type>(v));
        }


    private: 
       BaseString* m_base;
    };

    StringConcept* m_string;

    static StringConcept* make_properly_fitted_string(size_t char_size)
    {
        switch ( char_size ) {
        case sizeof( uint32_t ) :
            return new StringModel<std::basic_string<uint32_t, std::char_traits<uint32_t>, std::allocator<uint32_t>>>;
        case sizeof( uint16_t ) :
            return new StringModel<std::basic_string<uint16_t, std::char_traits<uint16_t>, std::allocator<uint16_t>>>;
        case sizeof( char ) :
            return new StringModel < std::string >;
        }
        throw std::invalid_argument( "Unknown char size" );
    }

    void refit( size_t char_size ) {
        if ( m_string->char_size() < char_size ) {
            StringConcept* model = make_properly_fitted_string( char_size );
            model->resize( m_string->size() );
            for ( size_t pos = 0; pos < model->size(); ++pos ) {
                model->set_at( pos, m_string->get_at( pos ) );
            }
            delete m_string;
            m_string = model;
        }
    }

    static char toUtf8( uint32_t value ) {
        // This is just a demo/placeholder
        return value >= 128 ? '?' : value;
    }


    template<typename CharT> 
    void copy_from_chars_inner( CharT* seq ) {
        while ( *seq ) {
            m_string->push_back( *seq );
            ++s;
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

public:
    VariantString():
        m_string(new StringModel<std::string>())
    {}

    template<typename CharT=char>
    VariantString(const CharT* s):
        m_string( new StringModel<std::basic_string<CharT, std::char_traits<CharT>, std::allocator<CharT>>>() )
    {
        copy_from_chars(s);
    }

    template<typename CharT = char>
    VariantString(const std::basic_string<CharT, std::char_traits<CharT>, std::allocator<CharT>>& s):
        m_string(new StringModel<std::basic_string<CharT, std::char_traits<CharT>, std::allocator<CharT>>>() )
    {
        *m_string = s;
    }

   ~VariantString() { delete m_string; }

    VariantString& operator=(const char* s)
    {
        clear();
        // Keep current char sizing
        m_string->copy_from( s );
    }


    VariantString& operator=( const VariantString& other ) {

    }

    size_t size() const { return m_string->size(); }
    void resize( size_t n ) { return m_string->resize( n ); }
    void clear() { return m_string->clear(); }
    const char* c_str() const { return m_string->c_str(); }
    
    void set_at(size_t pos, char chr) {
        // chars are always fitting
        m_string->set_at( pos, static_cast<uint32_t>(chr) );
    }

    void set_at( size_t pos, uint32_t chr ) {
        enlargeWhenCharTooLarge( chr );
        m_string->set_at( pos, static_cast<uint32_t>(chr) );
    }

    void push_back( char c ) {
        // chars are always fitting
        m_string->push_back( c ); 
    }

    void push_back(uint32_t chr) {
        enlargeWhenCharTooLarge( chr );
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
};
