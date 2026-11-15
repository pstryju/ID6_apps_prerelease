// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 nanaisu - NBT-AAD (https://github.com/Sq00ky/Supra-iDrive6-Android-Auto)
#include "cluster/hud_terminal.hpp"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

namespace nbt {
namespace hudterm {

namespace {



// ms left of a span that started at `from`, 0 once it has passed.
uint32_t left(uint32_t from, uint32_t span, uint32_t now) {
    uint32_t gone = now - from;
    return gone >= span ? 0 : span - gone;
}

} // namespace

const char* statusName(Status s) {
    switch (s) {
        case kOffline: return "offline";
        case kReady: return "ready";
        case kNavActive: return "factory-nav";
        case kFailed: return "failed";
    }
    return "?";
}

const char* submitResultName(SubmitResult r) {
    switch (r) {
        case kQueued: return "queued";
        case kReplaced: return "replaced";
        case kMerged: return "merged";
        case kNotReady: return "not-ready";
        case kRefused: return "refused";
    }
    return "?";
}

// ---- Core ----------------------------------------------------------------------------------------------------------

Core::Core(const Config& cfg)
    : cfg_(cfg), linkUp_(false), self_(0), host_(0), failed_(false), navActive_(false), routeOn_(false),
      frameOff_(false),
      lastNavMs_(0), status_(kOffline), changed_(false), haveCurrent_(false),
      nextCall_(0), batchStartMs_(0), recent_(cfg.replyMs), tag_(1), epoch_(0) {
    if (cfg_.maxInFlight == 0) cfg_.maxInFlight = 1;
    memset(&stats_, 0, sizeof(stats_));
}

void Core::dropWork(uint32_t now) {
    stats_.batchesDropped += (haveCurrent_ ? 1 : 0) + (uint32_t)pending_.size();
    if (haveCurrent_ || !pending_.empty()) ++epoch_;
    haveCurrent_ = false;
    current_ = Batch();
    pending_.clear();
    nextCall_ = 0;
    for (size_t i = 0; i < inFlight_.size(); ++i) {
        InFlight f = inFlight_[i];
        f.sentMs = now;
        dropped_.push_back(f);
    }
    inFlight_.clear();
}

// An answer to a call dropped within replyMs: taken off the list. Older entries are let go.
bool Core::takeDropped(uint16_t fkt, bool byHandle, uint16_t handle, uint32_t now) {
    size_t k = 0;
    for (size_t i = 0; i < dropped_.size(); ++i)
        if (left(dropped_[i].sentMs, cfg_.replyMs, now) > 0) dropped_[k++] = dropped_[i];
    dropped_.resize(k);
    for (size_t i = 0; i < dropped_.size(); ++i) {
        if (dropped_[i].fkt != fkt || (byHandle && dropped_[i].handle != handle)) continue;
        dropped_.erase(dropped_.begin() + i);
        return true;
    }
    return false;
}

void Core::refresh(const char* why, uint32_t now) {
    Status s = (!linkUp_ || !self_ || !host_) ? kOffline : failed_ ? kFailed
             : navActive_ ? kNavActive : kReady;
    if (s == status_) return;
    printf("[HudTerm] %s -> %s (%s)\n", statusName(status_), statusName(s), why);
    if (status_ == kReady) ++epoch_;          // what we drew may be gone or overdrawn: the driver redraws
    if (s != kReady) dropWork(now);
    status_ = s;
    changed_ = true;
}

void Core::fail(const char* why, uint32_t now) {
    printf("[HudTerm] FAILED: %s\n", why);
    failed_ = true;
    refresh(why, now);
}

// The navigation state is kept across a link change: a route that ends while the link is down leaves us yielded until
// the navigation's next route ends, which is the safe side.
void Core::setLink(bool up, uint16_t self, uint32_t now) {
    if (!up) {
        if (!linkUp_) return;
        linkUp_ = false;          // self_ stays, so late answers to our old address are not taken for the navigation
        refresh("link down", now);
        return;
    }
    if (!isAppAddress(self)) {
        printf("[HudTerm] link up but address %04X is not an app address: nothing will be sent\n", self);
        linkUp_ = false;
        refresh("no usable address", now);
        return;
    }
    if (linkUp_ && self == self_) return;
    linkUp_ = true;
    self_ = self;
    failed_ = false;
    dropWork(now);
    char why[48];
    snprintf(why, sizeof(why), "link up, address %04X", self);
    refresh(why, now);
}

void Core::setHost(uint16_t host, uint32_t now) {
    if (!isRingNode(host)) host = 0;
    if (host == host_) return;
    char why[64];
    if (host) snprintf(why, sizeof(why), "82/00 host %04X%s", host, host_ ? " (changed)" : "");
    else snprintf(why, sizeof(why), "no 82/00 host on a ring node in the registry");
    host_ = host;
    failed_ = false;
    navActive_ = false;
    routeOn_ = false;
    frameOff_ = false;
    recent_.clear();
    dropWork(now);
    dropped_.clear();                          // answers from the old host are not ours to match
    refresh(why, now);
}

void Core::reset(uint32_t now) {
    if (!failed_) return;
    failed_ = false;
    refresh("reset", now);
}

SubmitResult Core::submit(const Batch& b, uint32_t) {
    if (b.calls.empty() || b.calls.size() > cfg_.maxBatch) {
        ++stats_.refused;
        printf("[HudTerm] batch '%s' refused: %u calls (1..%u)\n", b.label.c_str(), (unsigned)b.calls.size(), cfg_.maxBatch);
        return kRefused;
    }
    for (size_t i = 0; i < b.calls.size(); ++i) {
        const Call& c = b.calls[i];
        if (callOk(c, cfg_.handles)) continue;
        ++stats_.refused;
        printf("[HudTerm] batch '%s' refused: call %u, %03X %s handle %u, %u bytes, breaks a limit\n", b.label.c_str(),
               (unsigned)i, c.fkt, fktName(c.fkt), c.handle(), (unsigned)c.payload.size());
        return kRefused;
    }
    if (status_ != kReady) return kNotReady;
    if (!haveCurrent_) {
        ++stats_.batches;
        current_ = b;
        haveCurrent_ = true;
        nextCall_ = 0;
        batchStartMs_ = 0;
        return kQueued;
    }
    if (b.slot == kSlotAll) {                         // a clear: nothing waiting is wanted any more
        bool any = !pending_.empty();
        ++stats_.batches;
        if (any) ++stats_.replaced;
        pending_.clear();
        pending_.push_back(b);
        return any ? kReplaced : kQueued;
    }
    size_t k = 0;
    while (k < pending_.size() && pending_[k].slot != b.slot) ++k;
    if (k == pending_.size()) {
        ++stats_.batches;
        pending_.push_back(b);
        return kQueued;
    }
    if (b.full) {                                     // to the back: what it is drawn on goes out first
        ++stats_.batches;
        ++stats_.replaced;
        pending_.erase(pending_.begin() + k);
        pending_.push_back(b);
        return kReplaced;
    }
    if (!merge(pending_[k], b)) {
        ++stats_.refused;
        printf("[HudTerm] batch '%s' refused: merged into '%s' it would be over %u calls\n", b.label.c_str(),
               pending_[k].label.c_str(), cfg_.maxBatch);
        return kRefused;
    }
    ++stats_.merged;
    return kMerged;
}

bool Core::waiting(int slot) const {
    for (size_t i = 0; i < pending_.size(); ++i)
        if (pending_[i].slot == slot || pending_[i].slot == kSlotAll) return true;
    return false;
}

// A partial batch into its slot's waiting one: a text for a handle it already sets replaces that payload where it
// stands (the newest text wins, in the waiting batch's order), anything else is appended. Nothing changes if the result
// would be too long.
bool Core::merge(Batch& into, const Batch& b) {
    Batch m = into;
    for (size_t i = 0; i < b.calls.size(); ++i) {
        const Call& c = b.calls[i];
        size_t k = 0;
        if (c.fkt == kTextareaSetText)
            while (k < m.calls.size() && !(m.calls[k].fkt == kTextareaSetText && m.calls[k].handle() == c.handle())) ++k;
        else
            k = m.calls.size();
        if (k < m.calls.size()) m.calls[k].payload = c.payload;
        else m.calls.push_back(c);
    }
    if (m.calls.size() > cfg_.maxBatch) return false;
    into = m;
    return true;
}

bool Core::next(uint32_t now, most::Msg& out) {
    if (status_ != kReady || !haveCurrent_ || nextCall_ >= current_.calls.size()) return false;
    if (inFlight_.size() >= cfg_.maxInFlight) return false;
    const Call& c = current_.calls[nextCall_];
    if (!buildCall(host_, c, tag_, cfg_.handles, out)) {   // checked at submit: only a host change could do this
        fail("a queued call no longer builds", now);
        return false;
    }
    if (nextCall_ == 0) batchStartMs_ = now;
    ++tag_;
    ++nextCall_;
    InFlight f;
    f.fkt = c.fkt;
    f.handle = c.handle();
    f.sentMs = now;
    inFlight_.push_back(f);
    recent_.add(c.fkt, f.handle, now);
    ++stats_.calls;
    return true;
}

void Core::sendFailed(uint32_t now) { fail("the MOST client did not send a call", now); }

void Core::finishIfDone(uint32_t now) {
    if (!haveCurrent_ || nextCall_ < current_.calls.size() || !inFlight_.empty()) return;
    ++stats_.batchesDone;
    if (cfg_.logBatches)
        printf("[HudTerm] batch '%s': %u calls answered in %u ms\n", current_.label.c_str(),
               (unsigned)current_.calls.size(), now - batchStartMs_);
    haveCurrent_ = false;
    if (pending_.empty()) {
        current_ = Batch();
        return;
    }
    current_ = pending_.front();
    pending_.erase(pending_.begin());
    haveCurrent_ = true;
    nextCall_ = 0;
}

void Core::onMessage(const most::View& m, uint32_t now) {
    if (m.fblock != kFBlock || m.inst != kInst || !host_) return;
    if (self_ && m.src == self_) return;                      // our own, if the server shows them to us
    if (self_ && m.dst == self_) {
        if (m.src == host_) onAnswer(m, now);
        return;
    }
    if (m.src != host_ && m.dst != host_) return;             // not this terminal
    if (m.src == host_ && recent_.isCopy(m.fkt, m.data, m.len, now)) {
        ++stats_.copies;
        return;
    }
    onNav(m, now);
}

// ErrorAck (9) carries the handle first, like every answer to an Ack method; Error (F) does not, so it matches on the
// function alone. An error fails the terminal even when it matches nothing in flight: one of our calls is wrong. Only
// an answer to a call we dropped a moment ago is let go (see the header).
void Core::onAnswer(const most::View& m, uint32_t now) {
    if (m.op == most::kOpProcessingAck || m.op == most::kOpProcessing) return;   // still working: the timeout stands
    bool error = m.op != most::kOpResultAck && m.op != most::kOpStatus;
    bool byHandle = m.op != most::kOpError && m.len >= 2;
    uint16_t h = byHandle ? rdBe16(m.data) : 0;
    size_t i = 0;
    while (i < inFlight_.size() && !(inFlight_[i].fkt == m.fkt && (!byHandle || inFlight_[i].handle == h))) ++i;
    bool matched = i < inFlight_.size();
    if (matched) inFlight_.erase(inFlight_.begin() + i);
    if (error && !matched && takeDropped(m.fkt, byHandle, h, now)) {
        ++stats_.lateErrors;
        printf("[HudTerm] %s(%X) to %03X %s handle %u, a call dropped when the terminal left ready: not a failure\n",
               most::opName(m.op), m.op, m.fkt, fktName(m.fkt), h);
        return;
    }
    if (error) {
        ++stats_.errors;
        char why[160];
        int n = snprintf(why, sizeof(why), "%s(%X) to %03X %s%s, payload", most::opName(m.op), m.op, m.fkt, fktName(m.fkt),
                         matched ? "" : " (matches no call in flight)");
        for (size_t k = 0; k < m.len && k < 12 && n < (int)sizeof(why) - 4; ++k)
            n += snprintf(why + n, sizeof(why) - n, " %02x", m.data[k]);
        fail(why, now);
        return;
    }
    if (!matched) {
        takeDropped(m.fkt, byHandle, h, now);
        ++stats_.strays;
        return;
    }
    ++stats_.answers;
    finishIfDone(now);
}

// Whether a navigation message (its call, or the cluster's answer with the call's payload) draws: a route is on screen.
// Clearing up (RemoveAllItems, empty texts, other instances switched off) is not drawing.
static bool draws(const most::View& m) {
    switch (m.fkt) {
        case kSwitchVisibility: case kAddPolygon: case kAddCircle: case kAddCircleDiameter: case 0xC05:   // ImageAddBitmap
            return true;
        case kTextareaSetText:                    // handle, encoding byte, then the text
            for (size_t i = 3; i < m.len; ++i)
                if (m.data[i]) return true;
            return false;
        default:
            return false;
    }
}

void Core::onNav(const most::View& m, uint32_t now) {
    ++stats_.navMsgs;
    lastNavMs_ = now;
    if (m.fkt == kSetVisibility && m.len >= 3 && cfg_.navFrame > 0 && rdBe16(m.data) == cfg_.navFrame) {
        routeOn_ = m.data[2] != 0;
        frameOff_ = !routeOn_;
    } else if (draws(m) && !frameOff_) {
        routeOn_ = true;
    }
    if (navActive_) return;
    navActive_ = true;
    char why[96];
    snprintf(why, sizeof(why), "factory navigation: %04X->%04X %03X %s %s(%X) len %u", m.src, m.dst, m.fkt, fktName(m.fkt),
             most::opName(m.op), m.op, (unsigned)m.len);
    refresh(why, now);
}

void Core::tick(uint32_t now) {
    if (status_ == kReady && !inFlight_.empty() && left(inFlight_[0].sentMs, cfg_.replyMs, now) == 0) {
        ++stats_.timeouts;
        char why[96];
        snprintf(why, sizeof(why), "no answer to %03X %s handle %u in %u ms", inFlight_[0].fkt, fktName(inFlight_[0].fkt),
                 inFlight_[0].handle, cfg_.replyMs);
        fail(why, now);
    }
    if (navActive_ && !routeOn_ && left(lastNavMs_, cfg_.navQuietMs, now) == 0) {
        navActive_ = false;
        frameOff_ = false;                     // the next drawing, frame-on seen or not, is a route again
        refresh("factory navigation finished: no route on screen, then quiet", now);
    }
}

uint32_t Core::msUntilDeadline(uint32_t now) const {
    uint32_t d = 0xFFFFFFFFu;
    if (status_ == kReady && !inFlight_.empty()) {
        uint32_t t = left(inFlight_[0].sentMs, cfg_.replyMs, now);
        if (t < d) d = t;
    }
    if (navActive_ && !routeOn_) {
        uint32_t t = left(lastNavMs_, cfg_.navQuietMs, now);
        if (t < d) d = t;
    }
    return d;
}

bool Core::takeStatusChange(Status& s) {
    if (!changed_) return false;
    changed_ = false;
    s = status_;
    return true;
}


} }
