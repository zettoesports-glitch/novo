#include "../source/ModernBMDRenderContext.h"

#include <cstdlib>
#include <iostream>

namespace
{
void Require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

ModernBMDRenderContext MakeContext(std::uint32_t slot,
                                   std::uint32_t generation,
                                   ModernBMDInstanceKind kind,
                                   std::uint32_t skeletonSlot)
{
    ModernBMDRenderContext context{};
    context.Draw.InstanceSlot = slot;
    context.Draw.InstanceGeneration = generation;
    context.Draw.ViewId = ModernRendererViewId::Main;
    context.Draw.PassId = ModernRendererPassId::Opaque;
    context.Kind = kind;
    context.AssetId = 0x1000u + slot;
    context.AssetGeneration = 3;
    context.SkeletonSlot = skeletonSlot;
    context.AllowModernDraw = true;
    return context;
}
}

int main()
{
    Require(!HasModernBMDRenderContext(), "context must start empty");

    ModernBMDRenderContext hero = MakeContext(4, 11, ModernBMDInstanceKind::LocalHero, 7);
    {
        CModernBMDRenderContextScope heroScope{hero};
        const ModernBMDRenderContext* current = GetModernBMDRenderContext();
        Require(current != nullptr, "hero scope must install a context");
        Require(current->Draw.InstanceSlot == 4, "hero instance slot mismatch");
        Require(current->Draw.InstanceGeneration == 11, "hero generation mismatch");
        Require(current->SkeletonSlot == 7, "hero skeleton slot mismatch");
        Require(current->Kind == ModernBMDInstanceKind::LocalHero, "hero kind mismatch");

        // Scope owns a snapshot. Later producer mutations cannot alter a draw
        // that is already traversing the BMD stack.
        hero.Draw.InstanceSlot = 99;
        Require(GetModernBMDRenderContext()->Draw.InstanceSlot == 4,
                "scope must snapshot producer context");

        ModernBMDRenderContext remote = MakeContext(19, 42, ModernBMDInstanceKind::RemotePlayer, 23);
        {
            CModernBMDRenderContextScope remoteScope{remote};
            current = GetModernBMDRenderContext();
            Require(current->Draw.InstanceSlot == 19, "nested remote slot mismatch");
            Require(current->Draw.InstanceGeneration == 42, "nested remote generation mismatch");
            Require(current->SkeletonSlot == 23, "nested remote skeleton slot mismatch");
            Require(current->Kind == ModernBMDInstanceKind::RemotePlayer, "nested remote kind mismatch");
        }

        current = GetModernBMDRenderContext();
        Require(current != nullptr, "outer context must be restored after nesting");
        Require(current->Draw.InstanceSlot == 4, "outer slot leaked after nested scope");
        Require(current->SkeletonSlot == 7, "outer skeleton slot leaked after nested scope");
    }

    Require(!HasModernBMDRenderContext(), "context must be cleared after outer scope");

    ModernBMDRenderContext preview = MakeContext(3, 2, ModernBMDInstanceKind::ItemPreview, 31);
    {
        CModernBMDRenderContextScope previewScope{preview};
        Require(GetModernBMDRenderContext()->Kind == ModernBMDInstanceKind::ItemPreview,
                "sibling scope inherited prior entity kind");
        Require(GetModernBMDRenderContext()->SkeletonSlot == 31,
                "sibling scope inherited prior skeleton slot");
    }

    Require(!HasModernBMDRenderContext(), "sibling scope must not leave state behind");
    return EXIT_SUCCESS;
}
