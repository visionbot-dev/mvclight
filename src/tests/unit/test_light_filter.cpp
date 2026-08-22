#include "light/light_filter.h"

#include "test_framework.h"

#include <cstring>
#include <string>

using mvclight::CBloomFilter;
using mvclight::kBloomUpdateAll;
using mvclight::kMaxFilterElements;

int main() {
    CBloomFilter f = CBloomFilter::Create(100, 0.001, 12345, kBloomUpdateAll);
    const uint8_t a[] = {'a', 'd', 'd', 'r', '1'};
    const uint8_t b[] = {'a', 'd', 'd', 'r', '2'};

    CHECK(!f.Contains(a, sizeof(a)));
    CHECK(f.Insert(a, sizeof(a)));
    CHECK(f.Contains(a, sizeof(a)));
    CHECK(!f.Contains(b, sizeof(b)));

    // 序列化：varstr 前缀 + 数据 + 4 + 4 + 1
    std::vector<uint8_t> out;
    CHECK(f.Serialize(out));
    CHECK(out.size() > 4 + 4 + 1 + 1);

    // 容量上限：第 20001 个元素拒绝
    CBloomFilter big = CBloomFilter::Create(kMaxFilterElements + 1, 0.001, 1);
    uint8_t data[8] = {0};
    bool last = false;
    for (size_t i = 0; i < kMaxFilterElements; ++i) {
        memcpy(data, &i, sizeof(i));
        last = big.Insert(data, sizeof(data));
    }
    CHECK(last);
    CHECK(big.IsFull());
    uint8_t extra[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0, 0, 0, 0};
    CHECK(!big.Insert(extra, sizeof(extra)));

    TEST_MAIN_RETURN();
}
