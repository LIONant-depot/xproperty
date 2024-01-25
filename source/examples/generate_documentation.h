#include <fstream>
#include <iostream>
#include <sstream>

namespace xproperty_doc
{
    struct memfile
    {
        template< typename...T_ARGS>
        void print( T_ARGS&&... Args )
        {
            if(m_Data.size() < (m_Index + 1024) )
                m_Data.resize(m_Data.size()+(1024*2));

            auto i = sprintf_s( &m_Data[m_Index], 1024, std::forward<T_ARGS&&>(Args)... );
            assert(i!=-1);
            m_Index += i;
            m_Data[m_Index] = 0;
        }

        void output() const
        {
            printf(m_Data.data());
        }

        constexpr std::string_view getStringView() const
        {
            return { m_Data.data(), static_cast<std::size_t>(m_Index-1) };
        }

        std::string       m_Title;
        int               m_Index = 0;
        std::vector<char> m_Data;
    };

    // collapsible_string_v
    inline constexpr static const char* collapsible_string_v =
R"(
<details><summary><i><b>[[[GROUP_NAME]]] Output </b>(Click to open) </i></summary>

~~~
[[[EXAMPLE_OUTPUT]]]
~~~
</details>
)";

    struct example_group
    {
        std::string             m_GroupName;
        std::vector<memfile>    m_Examples;
    };

    // Search and replace strings
    inline void ReplaceStr( std::string& str, const std::string& oldStr, const std::string_view& newStr )
    {
        std::string::size_type pos = 0u;
        while ((pos = str.find(oldStr, pos)) != std::string::npos)
        {
            str.replace(pos, oldStr.length(), newStr);
            pos += newStr.length();
        }
    }

    inline void Generate( std::vector<example_group>& ExampleGroup )
    {
        // Read the source file
        std::string Documentation;
        {
            std::ifstream inFile;
            inFile.open("../../source/examples/example_properties.h"); 

            std::stringstream strStream;
            strStream << inFile.rdbuf(); 
            Documentation = strStream.str();
        }

        // Insert all the outputs
        for( auto i = 0u; i< ExampleGroup[0].m_Examples.size(); ++i )
        {
            std::string Title = ExampleGroup[0].m_Examples[i].m_Title;

            // Terminate the example code
            std::string Output = "```";

            for( auto& G : ExampleGroup )
            {
                std::string tempOutput = collapsible_string_v;
                ReplaceStr(tempOutput, "[[[GROUP_NAME]]]",     G.m_GroupName);
                ReplaceStr(tempOutput, "[[[EXAMPLE_OUTPUT]]]", G.m_Examples[i].getStringView() );

                Output += "\n" + tempOutput;
            }

            ReplaceStr(Title, "struct ", "" );
            ReplaceStr(Title, "class ", "");
            ReplaceStr(Documentation, Title, Output );
        }

        // Convenient code replace with markdown
        ReplaceStr(Documentation, "cpp */", "```cpp");
        ReplaceStr(Documentation, "/*", "");

        // Save the file documentation
        {
            std::ofstream outFile;
            outFile.open("../../documentation/Documentation.md");
            outFile << Documentation;
        }
    }
}