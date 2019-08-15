#include <string>

class VariantString
{
public:
    class StringConcept  {
    private:
        virtual VariantString& operator=(const char* s) =0;
        virtual size_t size() = 0;
        virtual void resize (size_t n) =0;
        virtual void clear() =0;
        virtual char* c_str() =0;

        virtual char& operator[] (size_t pos) =0;
        virtual const char& operator[] (size_t pos) const =0;

        virtual VariantString& operator= (const VariantString& str) =0;
    };

    template<typename BaseString>
    class StringModel: public StringConcept {
    public:
        StringModel(BaseString& base): 
            m_base(base) 
        {}
        virtual VariantString& operator=(const char* s);
        virtual size_t size() ;
        virtual void resize (size_t n);
        virtual void clear();
        virtual char* c_str();

        virtual char& operator[] (size_t pos);
        virtual const char& operator[] (size_t pos) ;

        virtual VariantString& operator= (const VariantString& str);
    };

    StringConcept* m_base;
public:

    virtual VariantString& operator=(const char* s)
    {

    }
};
