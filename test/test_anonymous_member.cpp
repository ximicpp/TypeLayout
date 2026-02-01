// Test file to verify anonymous member support in TypeLayout signatures
#include <boost/typelayout/typelayout.hpp>
#include <cstdio>
#include <optional>
#include <variant>

using namespace boost::typelayout;

// Test struct with anonymous union
struct StructWithAnonUnion {
    int x;
    union {
        int a;
        float b;
    };
    int y;
};

// Test struct with anonymous struct
struct StructWithAnonStruct {
    int outer;
    struct {
        int inner_a;
        float inner_b;
    };
};

// Test struct with multiple anonymous members
struct MultipleAnon {
    int first;
    union { char c; short s; };
    int middle;
    union { double d; long long ll; };
    int last;
};

// Named union for comparison
struct StructWithNamedUnion {
    int x;
    union NamedUnion {
        int a;
        float b;
    } named;
    int y;
};

int main() {
    printf("=== Anonymous Member Support Test ===\n\n");
    
    // Test 1: Struct with anonymous union
    printf("1. StructWithAnonUnion:\n");
    printf("   Signature: %s\n", get_layout_signature_cstr<StructWithAnonUnion>());
    printf("   Expected: Contains '<anon:1>' for the anonymous union\n\n");
    
    // Test 2: Struct with anonymous struct
    printf("2. StructWithAnonStruct:\n");
    printf("   Signature: %s\n", get_layout_signature_cstr<StructWithAnonStruct>());
    printf("   Expected: Contains '<anon:...>' for the anonymous struct\n\n");
    
    // Test 3: Multiple anonymous members
    printf("3. MultipleAnon:\n");
    printf("   Signature: %s\n", get_layout_signature_cstr<MultipleAnon>());
    printf("   Expected: Contains '<anon:1>' and '<anon:3>' for the two unions\n\n");
    
    // Test 4: Named union (should NOT have <anon:>)
    printf("4. StructWithNamedUnion:\n");
    printf("   Signature: %s\n", get_layout_signature_cstr<StructWithNamedUnion>());
    printf("   Expected: Contains 'named' NOT '<anon:>'\n\n");
    
    // Test 5: std::optional (previously failed)
    printf("5. std::optional<int>:\n");
    printf("   Signature: %s\n", get_layout_signature_cstr<std::optional<int>>());
    printf("   Expected: Compiles without error!\n\n");
    
    // Test 6: std::variant (previously failed)
    printf("6. std::variant<int, float>:\n");
    using VariantIF = std::variant<int, float>;
    printf("   Signature: %s\n", get_layout_signature_cstr<VariantIF>());
    printf("   Expected: Compiles without error!\n\n");
    
    printf("=== All tests compiled successfully! ===\n");
    return 0;
}