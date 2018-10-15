// Copyright 2017-2018 ccls Authors
// SPDX-License-Identifier: Apache-2.0

#include "message_handler.h"
#include "pipeline.hh"
#include "query_utils.h"

#include <algorithm>
using namespace ccls;

namespace {
MethodType kMethodType = "textDocument/documentHighlight";

struct lsDocumentHighlight {
  enum Kind { Text = 1, Read = 2, Write = 3 };

  lsRange range;
  int kind = 1;

  // ccls extension
  Role role = Role::None;

  bool operator<(const lsDocumentHighlight &o) const {
    return !(range == o.range) ? range < o.range : kind < o.kind;
  }
};
MAKE_REFLECT_STRUCT(lsDocumentHighlight, range, kind, role);

struct In_TextDocumentDocumentHighlight : public RequestMessage {
  MethodType GetMethodType() const override { return kMethodType; }
  lsTextDocumentPositionParams params;
};
MAKE_REFLECT_STRUCT(In_TextDocumentDocumentHighlight, id, params);
REGISTER_IN_MESSAGE(In_TextDocumentDocumentHighlight);

struct Handler_TextDocumentDocumentHighlight
    : BaseMessageHandler<In_TextDocumentDocumentHighlight> {
  MethodType GetMethodType() const override { return kMethodType; }
  void Run(In_TextDocumentDocumentHighlight *request) override {
    int file_id;
    QueryFile *file;
    if (!FindFileOrFail(db, project, request->id,
                        request->params.textDocument.uri.GetPath(), &file,
                        &file_id))
      return;

    WorkingFile *wfile = working_files->GetFileByFilename(file->def->path);
    std::vector<lsDocumentHighlight> result;

    std::vector<SymbolRef> syms =
        FindSymbolsAtLocation(wfile, file, request->params.position, true);
    for (auto [sym, refcnt] : file->symbol2refcnt) {
      if (refcnt <= 0)
        continue;
      Usr usr = sym.usr;
      SymbolKind kind = sym.kind;
      if (std::none_of(syms.begin(), syms.end(), [&](auto &sym1) {
            return usr == sym1.usr && kind == sym1.kind;
          }))
        continue;
      if (auto loc = GetLsLocation(db, working_files, sym, file_id)) {
        lsDocumentHighlight highlight;
        highlight.range = loc->range;
        if (sym.role & Role::Write)
          highlight.kind = lsDocumentHighlight::Write;
        else if (sym.role & Role::Read)
          highlight.kind = lsDocumentHighlight::Read;
        else
          highlight.kind = lsDocumentHighlight::Text;
        highlight.role = sym.role;
        result.push_back(highlight);
      }
    }
    std::sort(result.begin(), result.end());
    pipeline::Reply(request->id, result);
  }
};
REGISTER_MESSAGE_HANDLER(Handler_TextDocumentDocumentHighlight);
} // namespace
