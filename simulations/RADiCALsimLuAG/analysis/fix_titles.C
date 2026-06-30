// fix_titles.C — rename "front #minus back" -> "downstream #minus upstream" in
// the DeltaT histogram title of every per-energy scan file, in place.
//   root -l -b -q 'analysis/fix_titles.C("build/scan_opt")'
#include <TSystemDirectory.h>

void fixOne(const char* path) {
  TFile* f = TFile::Open(path, "UPDATE");
  if (!f || f->IsZombie()) { printf("  skip (cannot open) %s\n", path); return; }
  TH1* h = (TH1*)f->Get("DeltaT");
  if (h) {
    TString full = TString::Format("%s;%s;%s",
        h->GetTitle(), h->GetXaxis()->GetTitle(), h->GetYaxis()->GetTitle());
    full.ReplaceAll("front #minus back", "downstream #minus upstream");
    full.ReplaceAll("front-back", "downstream-upstream");
    h->SetTitle(full);
    h->Write("", TObject::kOverwrite);
    printf("  fixed DeltaT title in %s\n", path);
  }
  f->Close();
}

void fix_titles(const char* dir="build/scan_opt") {
  TSystemDirectory d(dir, dir);
  TList* files = d.GetListOfFiles();
  if (!files) { printf("no dir %s\n", dir); return; }
  TIter it(files); TSystemFile* sf;
  while ((sf = (TSystemFile*)it())) {
    TString n = sf->GetName();
    if (n.BeginsWith("radical_E") && n.EndsWith(".root"))
      fixOne(Form("%s/%s", dir, n.Data()));
  }
}
