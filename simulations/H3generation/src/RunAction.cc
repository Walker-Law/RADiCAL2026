#include "RunAction.hh"
#include "G4Run.hh"
#include "G4AccumulableManager.hh"
#include "G4AnalysisManager.hh"
#include "G4SystemOfUnits.hh"

#include "TFile.h"
#include "TTree.h"

#include <iomanip>

RunAction::RunAction() {
    G4AccumulableManager* am = G4AccumulableManager::Instance();
    am->RegisterAccumulable(fTritonsBe);
    am->RegisterAccumulable(fTritonsW);
    am->RegisterAccumulable(fTritonsBlanket);
    am->RegisterAccumulable(fEnterBe);
    am->RegisterAccumulable(fEnterW);
    am->RegisterAccumulable(fEnterBlanket);
    am->RegisterAccumulable(fNeutronInteractions);
    am->RegisterAccumulable(fNeutronInteractionsBlanket);

    auto* ana = G4AnalysisManager::Instance();
    ana->SetDefaultFileType("root");
    ana->SetFileName("H3generation");
    ana->SetVerboseLevel(0);

    // Spectra histograms (indices 0–4) — filled per step
    ana->CreateH1("nSpec_Be",      "Neutron KE entering Be (MeV)",      150, 0., 15.);
    ana->CreateH1("nSpec_W",       "Neutron KE entering W (MeV)",       150, 0., 15.);
    ana->CreateH1("nSpec_Blanket", "Neutron KE entering Blanket (MeV)", 150, 0., 15.);
    ana->CreateH1("triton_KE",     "Triton KE at production (MeV)",     100, 0.,  5.);
    ana->CreateH1("triton_R",      "Triton production radius (cm)",       70, 100., 170.);

    // Count histograms (indices 5–6) — filled per occurrence, bin content = total count
    ana->CreateH1("nTritons_total",      "H3 produced (fill once per triton)",               1, 0., 1.);
    ana->CreateH1("nNeutronInt_blanket", "Neutron interactions in blanket (fill per event)",  1, 0., 1.);
}

void RunAction::BeginOfRunAction(const G4Run*) {
    G4AccumulableManager::Instance()->Reset();
    G4AnalysisManager::Instance()->OpenFile();
}

void RunAction::AddTriton(const G4String& vol, G4double ke, G4double r) {
    if      (vol == "Be_Shell") fTritonsBe      += 1;
    else if (vol == "W_Shell")  fTritonsW       += 1;
    else if (vol == "Blanket")  fTritonsBlanket += 1;

    auto* ana = G4AnalysisManager::Instance();
    ana->FillH1(3, ke / MeV);   // triton_KE
    ana->FillH1(4, r  / cm);    // triton_R
    ana->FillH1(5, 0.5);        // nTritons_total: increment by 1 per triton
}

void RunAction::AddParticleEntering(const G4String& vol, G4double ke) {
    auto* ana = G4AnalysisManager::Instance();
    if (vol == "Be_Shell") {
        fEnterBe += 1;
        ana->FillH1(0, ke / MeV);
    } else if (vol == "W_Shell") {
        fEnterW += 1;
        ana->FillH1(1, ke / MeV);
    } else if (vol == "Blanket") {
        fEnterBlanket += 1;
        ana->FillH1(2, ke / MeV);
    }
}

void RunAction::AddNeutronInteraction(const G4String& vol) {
    fNeutronInteractions += 1;
    if (vol == "Blanket") {
        fNeutronInteractionsBlanket += 1;
        G4AnalysisManager::Instance()->FillH1(6, 0.5);  // nNeutronInt_blanket: increment per hit
    }
}

void RunAction::EndOfRunAction(const G4Run* run) {
    G4AccumulableManager::Instance()->Merge();

    G4int nSrc        = run->GetNumberOfEvent();
    G4int tBe         = fTritonsBe.GetValue();
    G4int tW          = fTritonsW.GetValue();
    G4int tBlank      = fTritonsBlanket.GetValue();
    G4int tTotal      = tBe + tW + tBlank;
    G4int nInt        = fNeutronInteractions.GetValue();
    G4int nIntBlanket = fNeutronInteractionsBlanket.GetValue();
    G4int eBe         = fEnterBe.GetValue();
    G4int eW          = fEnterW.GetValue();
    G4int eBlank      = fEnterBlanket.GetValue();

    auto ratio = [](G4int num, G4int den) -> Double_t {
        return den > 0 ? (Double_t)num / den : 0.0;
    };

    // Write spectra + count histograms via G4AnalysisManager
    auto* ana = G4AnalysisManager::Instance();
    ana->Write();
    ana->CloseFile();

    // Append scalar summary as a proper TTree to the same ROOT file
    {
        TFile f("H3generation.root", "UPDATE");
        TTree* t = new TTree("scalars", "Run scalar summary");

        Int_t    v_nSrc        = nSrc;
        Int_t    v_tTotal      = tTotal;
        Int_t    v_nInt        = nInt;
        Int_t    v_nIntBlanket = nIntBlanket;
        Double_t v_tbrSrc      = ratio(tTotal, nSrc);
        Double_t v_tbrInt      = ratio(tTotal, nInt);
        Double_t v_tbrBlanket  = ratio(tTotal, nIntBlanket);

        t->Branch("nSourceNeutrons",          &v_nSrc,        "nSourceNeutrons/I");
        t->Branch("nTritonsTotal",            &v_tTotal,      "nTritonsTotal/I");
        t->Branch("nNeutronInteractions",     &v_nInt,        "nNeutronInteractions/I");
        t->Branch("nNeutronIntBlanket",       &v_nIntBlanket, "nNeutronIntBlanket/I");
        t->Branch("TBR_per_source",           &v_tbrSrc,      "TBR_per_source/D");
        t->Branch("TBR_per_interaction",      &v_tbrInt,      "TBR_per_interaction/D");
        t->Branch("TBR_per_blanket_int",      &v_tbrBlanket,  "TBR_per_blanket_int/D");
        t->Fill();
        t->Write("", TObject::kOverwrite);
        f.Close();
    }

    G4cout << "\n======================================================\n";
    G4cout << "  TRITIUM (H-3) PRODUCTION SUMMARY\n";
    G4cout << "======================================================\n";
    G4cout << std::left << std::setw(34) << "  Source neutrons:"                << nSrc        << "\n";
    G4cout << std::left << std::setw(34) << "  Total tritons:"                  << tTotal      << "\n";
    G4cout << std::left << std::setw(34) << "  Neutron interactions (all):"     << nInt        << "\n";
    G4cout << std::left << std::setw(34) << "  Neutron interactions (blanket):" << nIntBlanket << "\n";
    G4cout << std::left << std::setw(34) << "  TBR (tritons/source):"
           << std::fixed << std::setprecision(5) << ratio(tTotal, nSrc) << "\n";
    G4cout << std::left << std::setw(34) << "  TBR (tritons/n-int all):"
           << std::fixed << std::setprecision(5) << ratio(tTotal, nInt) << "\n";
    G4cout << std::left << std::setw(34) << "  TBR (tritons/n-int blanket):"
           << std::fixed << std::setprecision(5) << ratio(tTotal, nIntBlanket) << "\n";
    G4cout << "------------------------------------------------------\n";
    G4cout << "  Layer            Tritons  Entering   H3/Entering\n";
    G4cout << "------------------------------------------------------\n";
    G4cout << std::left << std::setw(17) << "  Be multiplier"
           << std::setw(9) << tBe    << std::setw(11) << eBe
           << std::fixed << std::setprecision(5) << ratio(tBe, eBe) << "\n";
    G4cout << std::left << std::setw(17) << "  Tungsten layer"
           << std::setw(9) << tW     << std::setw(11) << eW
           << std::fixed << std::setprecision(5) << ratio(tW, eW) << "\n";
    G4cout << std::left << std::setw(17) << "  Li4SiO4 blanket"
           << std::setw(9) << tBlank << std::setw(11) << eBlank
           << std::fixed << std::setprecision(5) << ratio(tBlank, eBlank) << "\n";
    G4cout << "======================================================\n";
    G4cout << "  ROOT output: H3generation.root\n";
    G4cout << "======================================================\n\n";
}
