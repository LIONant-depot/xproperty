// Standalone self-check of xproperty::keyed_vector (xcore/my_properties.h): a list addressed by its elements' keys. Build from the xLION root
// with /std:c++latest /EHsc /I. so "dependencies/..." resolves:
//   cl /std:c++latest /EHsc /O2 /I. dependencies/xproperty/source/xcore/keyed_vector_test.cpp
//  - by path: "List[G:key]/V" finds the element whatever its place; a key not there is added at the end
//  - order: the vector keeps the order it was made in; the collector walks it in that order and names each element by its key
//  - a text round trip: saved in order, loaded with no search (each find hits its hint)
//  - timing: a load in order against one in a shuffled order (each find a linear search)
#include "dependencies/xproperty/source/xcore/my_properties.cpp"
#include "dependencies/xtextfile/source/xtextfile.h"
#include "dependencies/xproperty/source/sprop/property_sprop_xtextfile_serializer.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <random>
#include <algorithm>

namespace
{
    struct item
    {
        std::uint64_t   m_Key = 0;
        float           m_V   = 0;
        XPROPERTY_DEF
        ( "item", item
        , obj_member<"Key", &item::m_Key>
        , obj_member<"V",   &item::m_V>
        )
    };
    XPROPERTY_REG(item)

    struct holder
    {
        xproperty::keyed_vector<item, &item::m_Key> m_List;
        XPROPERTY_DEF
        ( "holder", holder
        , obj_member<"List", &holder::m_List>
        )
    };
    XPROPERTY_REG(holder)

    struct plain_holder         // the same data in a plain std::vector (addressed by place): what a load costs without keys
    {
        std::vector<item> m_List;
        XPROPERTY_DEF
        ( "plain_holder", plain_holder
        , obj_member<"List", &plain_holder::m_List>
        )
    };
    XPROPERTY_REG(plain_holder)

    bool Set(holder& H, std::uint64_t Key, float V)
    {
        std::string                  Error;
        xproperty::settings::context C{};
        xproperty::any               A; A.set<float>(V);
        xproperty::sprop::setProperty(Error, &H, *xproperty::getObject(H), xproperty::sprop::container::prop{ std::format("holder/List[G:{}]/V", Key), A }, C);
        return Error.empty();
    }

    double Ms(std::chrono::steady_clock::time_point A, std::chrono::steady_clock::time_point B) { return std::chrono::duration<double, std::milli>(B - A).count(); }
}

int main()
{
    // By path, whatever the place; a missing key is added at the end
    {
        holder H;
        H.m_List.findOrAdd(30).m_V = 3;
        H.m_List.findOrAdd(10).m_V = 1;
        H.m_List.findOrAdd(20).m_V = 2;
        assert(Set(H, 10, 11) && H.m_List[1].m_V == 11);
        assert(Set(H, 99, 9) && H.m_List.size() == 4 && H.m_List[3].m_Key == 99 && H.m_List[3].m_V == 9);
        assert(H.m_List.find(20) == &H.m_List[2] && H.m_List.find(7) == nullptr);

        // the collector names each element by its key, in the vector's order
        std::vector<std::string> Paths;
        xproperty::settings::context C{};
        xproperty::sprop::collector(H, C, [&](const char* pName, xproperty::any&&, const xproperty::type::members&, bool, const void*) noexcept { if (std::string_view(pName).ends_with("/V")) Paths.push_back(pName); });
        assert(Paths.size() == 4 && Paths[0] == "holder/List[G:30]/V" && Paths[1] == "holder/List[G:10]/V" && Paths[3] == "holder/List[G:99]/V");

        // a file of the old form, by place: the element's own Key property rekeys it, its other properties still reach it
        {
            holder O;
            xproperty::settings::context C2{};
            std::string Error;
            const auto SetAny = [&](const char* pPath, xproperty::any A) { xproperty::sprop::setProperty(Error, &O, *xproperty::getObject(O), xproperty::sprop::container::prop{ pPath, A }, C2); };
            xproperty::any K; K.set<std::uint64_t>(777);
            xproperty::any V; V.set<float>(7.0f);
            SetAny("holder/List[G:0]/Key", K);
            SetAny("holder/List[G:0]/V",   V);
            K.set<std::uint64_t>(888); V.set<float>(8.0f);
            SetAny("holder/List[G:1]/Key", K);
            SetAny("holder/List[G:1]/V",   V);
            assert(O.m_List.size() == 2 && O.m_List[0].m_Key == 777 && O.m_List[0].m_V == 7 && O.m_List[1].m_Key == 888 && O.m_List[1].m_V == 8);
        }

        // the order changes, the address does not
        std::swap(H.m_List[0], H.m_List[2]);
        assert(Set(H, 30, 33) && H.m_List[2].m_Key == 30 && H.m_List[2].m_V == 33);
    }

    // A text round trip and the timing
    for (const int N : { 100, 10000 })
    {
        holder H;
        std::mt19937_64 Rng(1234);
        for (int i = 0; i < N; ++i) H.m_List.findOrAdd(Rng() | 1).m_V = float(i);
        const auto Path = (std::filesystem::temp_directory_path() / L"keyed_vector_test.txt").wstring();
        xproperty::settings::context C{};
        auto t0 = std::chrono::steady_clock::now();
        {
            xtextfile::stream F;
            assert(!F.Open(false, Path, xtextfile::file_type::TEXT));
            assert(!xproperty::sprop::serializer::Stream(F, H, C));
        }
        auto t1 = std::chrono::steady_clock::now();
        holder R;
        {
            xtextfile::stream F;
            assert(!F.Open(true, Path, xtextfile::file_type::TEXT));
            assert(!xproperty::sprop::serializer::Stream(F, R, C));
        }
        auto t2 = std::chrono::steady_clock::now();
        double PlainLoadMs = 0;
        {
            plain_holder P;
            P.m_List.assign(H.m_List.begin(), H.m_List.end());
            const auto PlainPath = (std::filesystem::temp_directory_path() / L"keyed_vector_test_plain.txt").wstring();
            { xtextfile::stream F; assert(!F.Open(false, PlainPath, xtextfile::file_type::TEXT)); assert(!xproperty::sprop::serializer::Stream(F, P, C)); }
            plain_holder Q;
            const auto a = std::chrono::steady_clock::now();
            { xtextfile::stream F; assert(!F.Open(true, PlainPath, xtextfile::file_type::TEXT)); assert(!xproperty::sprop::serializer::Stream(F, Q, C)); }
            PlainLoadMs = Ms(a, std::chrono::steady_clock::now());
            assert(Q.m_List.size() == std::size_t(N));
        }
        assert(R.m_List.size() == std::size_t(N));
        for (int i = 0; i < N; ++i) assert(R.m_List[i].m_Key == H.m_List[i].m_Key && R.m_List[i].m_V == H.m_List[i].m_V);
        assert(R.m_List.m_nSearches == 0);         // loaded in order: every find hit its hint

        // set by path in order (the hint) and shuffled (searches)
        std::vector<std::uint64_t> Keys;
        for (auto& E : H.m_List) Keys.push_back(E.m_Key);
        auto t3 = std::chrono::steady_clock::now();
        H.m_List.m_Hint = 0; H.m_List.m_nSearches = 0;
        for (auto K : Keys) Set(H, K, 1.0f);
        auto t4 = std::chrono::steady_clock::now();
        const auto nInOrder = H.m_List.m_nSearches;
        std::shuffle(Keys.begin(), Keys.end(), Rng);
        H.m_List.m_nSearches = 0;
        for (auto K : Keys) Set(H, K, 2.0f);
        auto t5 = std::chrono::steady_clock::now();
        std::printf("N=%5d  save %.2f ms, load %.2f ms (%zu searches; a plain std::vector %.2f ms); set by path: in order %.2f ms (%zu searches), shuffled %.2f ms (%zu searches)\n"
                   , N, Ms(t0, t1), Ms(t1, t2), R.m_List.m_nSearches, PlainLoadMs, Ms(t3, t4), nInOrder, Ms(t4, t5), H.m_List.m_nSearches);
    }

    std::printf("keyed_vector: PASS\n");
    return 0;
}
