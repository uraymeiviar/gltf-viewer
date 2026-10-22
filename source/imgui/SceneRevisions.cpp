// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/SceneRevisions.h"

#include <cstring>
#include <sstream>

namespace ux3d::imgui {

SelfCheckResult SceneRevisions::runSelfCheck(bool forceFailure)
{
    SelfCheckResult result;
    std::ostringstream report;
    int failures = 0;
    int assertions = 0;
    auto check = [&](bool condition, const std::string& label) {
        ++assertions;
        report << (condition ? "PASS " : "FAIL ") << label << "\n";
        if (!condition) ++failures;
    };

    report << "Scene revision contract self-check\n";

    SceneRevisions revisions;
    check(!revisions.hasPending(), "fresh: nothing pending");
    check(revisions.markCount() == 0, "fresh: zero marks");
    for (size_t i = 0; i < kSceneChangeCount; ++i) {
        const auto change = static_cast<SceneChange>(i);
        check(revisions.revision(change) == 0, std::string("fresh: revision 0 for ") + sceneChangeName(change));
        check(std::strcmp(sceneChangeName(change), "Unknown") != 0, std::string("named kind ") + std::to_string(i));
    }

    // Each kind sets only its own bit and revision.
    for (size_t i = 0; i < kSceneChangeCount; ++i) {
        const auto change = static_cast<SceneChange>(i);
        SceneRevisions single;
        single.mark(change);
        check(single.pendingMask() == sceneChangeBit(change), std::string("isolated bit for ") + sceneChangeName(change));
        bool othersUntouched = true;
        for (size_t j = 0; j < kSceneChangeCount; ++j) {
            if (j != i && single.revision(static_cast<SceneChange>(j)) != 0) othersUntouched = false;
        }
        check(single.revision(change) == 1 && othersUntouched, std::string("isolated revision for ") + sceneChangeName(change));
    }

    // Repeated marks collapse in the mask but advance the revision.
    revisions.mark(SceneChange::Selection);
    revisions.mark(SceneChange::Selection);
    revisions.mark(SceneChange::Transform);
    check(revisions.pendingMask() == (sceneChangeBit(SceneChange::Selection) | sceneChangeBit(SceneChange::Transform)),
          "repeated marks collapse in pending mask");
    check(revisions.revision(SceneChange::Selection) == 2, "repeated marks advance revision");
    check(revisions.markCount() == 3, "mark count includes repeats");
    check(revisions.isPending(SceneChange::Transform) && !revisions.isPending(SceneChange::Material), "isPending per kind");
    check(SceneRevisions::maskToString(revisions.pendingMask()) == "Transform|Selection", "mask names in enum order");

    // Consume returns and clears pending kinds; revisions persist.
    const uint32_t consumed = revisions.consumePending();
    check(consumed == (sceneChangeBit(SceneChange::Selection) | sceneChangeBit(SceneChange::Transform)), "consume returns pending mask");
    check(!revisions.hasPending() && revisions.consumePending() == 0, "consume clears pending");
    check(revisions.revision(SceneChange::Selection) == 2 && revisions.revision(SceneChange::Transform) == 1,
          "revisions survive consume");

    // A mark after consume is pending for the next consumer (marks made after the
    // frame's sync point carry into the following frame).
    revisions.mark(SceneChange::AnimationTime);
    check(revisions.pendingMask() == sceneChangeBit(SceneChange::AnimationTime), "post-consume mark carries to next consume");

    // Out-of-range input is ignored.
    revisions.mark(SceneChange::Count);
    check(revisions.pendingMask() == sceneChangeBit(SceneChange::AnimationTime) && revisions.revision(SceneChange::Count) == 0,
          "Count sentinel ignored");

    // Renderer sync mask: everything except Selection, ViewCamera (P4.2) and AnimationTime (P4.3).
    check((kRendererSyncChanges & sceneChangeBit(SceneChange::Selection)) == 0, "Selection does not require renderer sync");
    check((kRendererSyncChanges & sceneChangeBit(SceneChange::ViewCamera)) == 0, "ViewCamera does not require renderer sync");
    check((kRendererSyncChanges & sceneChangeBit(SceneChange::SceneCamera)) != 0, "SceneCamera still requires renderer sync");
    check((kRendererSyncChanges & sceneChangeBit(SceneChange::AnimationTime)) == 0, "AnimationTime does not require renderer sync");
    check((kRendererSyncChanges & sceneChangeBit(SceneChange::AnimationSet)) != 0, "AnimationSet still requires renderer sync");
    bool othersRequireSync = true;
    for (size_t i = 0; i < kSceneChangeCount; ++i) {
        const auto change = static_cast<SceneChange>(i);
        if (change == SceneChange::Selection || change == SceneChange::ViewCamera || change == SceneChange::AnimationTime) continue;
        if ((kRendererSyncChanges & sceneChangeBit(change)) == 0) othersRequireSync = false;
    }
    check(othersRequireSync, "every other kind still requires renderer sync");
    check((kRendererSyncChanges & ~kAllSceneChanges) == 0, "sync mask has no bits beyond the defined kinds");

    // noteSynced (P4.3): an already-synced revision is excluded from the
    // unsynced mask but stays pending; a later mark makes it unsynced again.
    SceneRevisions loaded;
    check(loaded.unsyncedPendingMask() == 0, "fresh: nothing unsynced");
    loaded.mark(SceneChange::Topology);
    check(loaded.unsyncedPendingMask() == sceneChangeBit(SceneChange::Topology), "marked kind is unsynced");
    loaded.noteSynced(SceneChange::Topology);
    check(loaded.unsyncedPendingMask() == 0 && loaded.isPending(SceneChange::Topology), "noted kind is synced but still pending");
    loaded.mark(SceneChange::Visibility);
    check(loaded.unsyncedPendingMask() == sceneChangeBit(SceneChange::Visibility), "other kinds stay unsynced");
    loaded.mark(SceneChange::Topology);
    check((loaded.unsyncedPendingMask() & sceneChangeBit(SceneChange::Topology)) != 0, "a later mark of the noted kind is unsynced again");
    loaded.consumePending();
    loaded.noteSynced(SceneChange::Topology);
    check(loaded.unsyncedPendingMask() == 0, "consume clears unsynced mask");

    if (forceFailure) check(false, "forced failure");

    report << "assertions=" << assertions << " failures=" << failures << "\n";
    result.success = failures == 0;
    result.report = report.str();
    return result;
}

} // namespace ux3d::imgui
