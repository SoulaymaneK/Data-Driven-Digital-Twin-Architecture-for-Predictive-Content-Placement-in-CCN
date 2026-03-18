#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"
#include <vector>
#include <algorithm>
#include <iostream>
#include <iomanip>

using namespace ns3;

/**
 * STRUCTURE DE DONNÉES
 */
struct VideoItem {
    int id;             // nom de la vidéo
    int taille;         // Size(i)
    double popularite;  // p_hat
    double hit_value;   // Gain
};

// Fonction de comparaison
bool compareVideos(const VideoItem& a, const VideoItem& b) {
    double scoreA = (a.popularite * a.hit_value) / a.taille;
    double scoreB = (b.popularite * b.hit_value) / b.taille;
    return scoreA > scoreB; 
}

int main(int argc, char* argv[]) {
    // ETAPE 1 : TOPOLOGIE
    NodeContainer clients; clients.Create(3);
    NodeContainer superNoeud; superNoeud.Create(1);
    NodeContainer serveur; serveur.Create(1);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));

    p2p.Install(clients.Get(0), superNoeud.Get(0));
    p2p.Install(clients.Get(1), superNoeud.Get(0));
    p2p.Install(clients.Get(2), superNoeud.Get(0));
    p2p.Install(superNoeud.Get(0), serveur.Get(0));

    ns3::ndn::StackHelper ndnHelper;
    ndnHelper.SetDefaultRoutes(true);
    ndnHelper.setCsSize(10); // Capacité du cache = 10
    
    ndnHelper.Install(clients);
    ndnHelper.Install(serveur);
    ndnHelper.Install(superNoeud);

    // ETAPE 2 : DONNÉES & ALGORITHME DT (Logs détaillés)
    
    std::vector<VideoItem> catalogue = {
        {1, 2, 0.9, 10.0}, 
        {2, 8, 0.1, 10.0}, 
        {3, 3, 0.5, 10.0},
        {4, 2, 0.8, 10.0} 
    };

    int capacite_totale = 10;
    int capacite_actuelle = capacite_totale;
    std::vector<int> videos_a_cacher;

    // --- DEBUT DES LOGS ALGORITHMIQUES ---
    std::cout << "[SYSTEM] INITIALISATION DU MODULE DIGITAL TWIN" << std::endl;
    std::cout << "[PARAM] Capacite Cache Super-Noeud : " << capacite_totale << " blocs" << std::endl;
    std::cout << "[PARAM] Formule de Score : (Probabilite * Gain) / Taille\n" << std::endl;

    // 1. TRI
    std::sort(catalogue.begin(), catalogue.end(), compareVideos);

    std::cout << "[ETAPE 1] CLASSEMENT PAR RENTABILITE (SORTING PROCESS)" << std::endl;
    std::cout << "Rang | ID Video | Taille | Probabilite | Score Calcule" << std::endl;
    std::cout << "-----|----------|--------|-------------|--------------" << std::endl;
    
    int rank = 1;
    for(const auto& v : catalogue) {
        double score = (v.popularite * v.hit_value) / v.taille;
        std::cout << "  " << rank++ << "  |    " << v.id << "     |   " << v.taille 
                  << "    |    " << std::fixed << std::setprecision(1) << v.popularite 
                  << "      |     " << score << std::endl;
    }
    std::cout << std::endl;

    // 2. SELECTION (Heuristique Gloutonne)
    std::cout << "[ETAPE 2] EXECUTION HEURISTIQUE GLOUTONNE (GREEDY ALLOCATION)" << std::endl;

    for(const auto& video : catalogue) {
        std::cout << "[INFO] Traitement Candidat ID: " << video.id << " (Score: " << (video.popularite * video.hit_value) / video.taille << ")" << std::endl;
        std::cout << "       - Espace Requis : " << video.taille << std::endl;
        std::cout << "       - Espace Dispo  : " << capacite_actuelle << std::endl;

        if (video.taille <= capacite_actuelle) {
            std::cout << "       -> TEST CAPACITE : OK" << std::endl;
            std::cout << "       -> ACTION : INSERTION DANS LISTE DE PRE-CHARGEMENT" << std::endl;
            
            videos_a_cacher.push_back(video.id);
            capacite_actuelle -= video.taille;
            
            std::cout << "       -> ETAT : Capacite restante mise a jour = " << capacite_actuelle << std::endl;
        } else {
            std::cout << "       -> TEST CAPACITE : ECHEC (Overflow)" << std::endl;
            std::cout << "       -> ACTION : REJET DU CANDIDAT" << std::endl;
        }
        std::cout << "--------------------------------------------------------" << std::endl;
    }

    std::cout << "[RESUME] Fin de l'allocation." << std::endl;
    std::cout << "[OUTPUT] Contenus a cacher : [ ";
    for(int id : videos_a_cacher) std::cout << id << " ";
    std::cout << "]" << std::endl;
    std::cout << "[OUTPUT] Taux d'occupation final : " << ((double)(capacite_totale - capacite_actuelle)/capacite_totale)*100 << "%\n" << std::endl;


    // ---------------------------------------------------------
    // ETAPE 3 : APPLICATION (Identique)
    // ---------------------------------------------------------
    ns3::ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
    consumerHelper.SetAttribute("Frequency", StringValue("10")); 

    for(int vid_id : videos_a_cacher) {
        std::string prefix = "/prefix/video-" + std::to_string(vid_id);
        consumerHelper.SetPrefix(prefix);
        consumerHelper.Install(clients.Get(0)).Start(Seconds(0.1));
        consumerHelper.Install(clients.Get(0)).Stop(Seconds(0.5)); 
    }

    // ---------------------------------------------------------
    // ETAPE 4 : SIMULATION (Identique)
    // ---------------------------------------------------------
    ns3::ndn::AppHelper producerHelper("ns3::ndn::Producer");
    producerHelper.SetPrefix("/prefix");
    producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
    producerHelper.Install(serveur);

    consumerHelper.SetPrefix("/prefix/video-1");
    consumerHelper.Install(clients.Get(1)).Start(Seconds(2.0));

    consumerHelper.SetPrefix("/prefix/video-2");
    consumerHelper.Install(clients.Get(2)).Start(Seconds(3.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}