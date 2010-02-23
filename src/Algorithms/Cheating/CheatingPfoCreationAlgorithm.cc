/**
 *  @file   PandoraPFANew/src/Algorithms/CheatingCheatingPfoCreationAlgorithm.cc
 * 
 *  @brief  Implementation of the cheating pfo creation algorithm class.
 * 
 *  $Log: $
 */

#include "Algorithms/Cheating/CheatingPfoCreationAlgorithm.h"

#include "Api/PandoraContentApi.h"

#include "Objects/CaloHit.h"
#include "Objects/MCParticle.h"
#include "Objects/Cluster.h"
#include "Objects/CartesianVector.h"

#include <sstream>
#include <cmath>

using namespace pandora;

StatusCode CheatingPfoCreationAlgorithm::Run()
{
    // Run initial clustering algorithm
    const ClusterList *pClusterList = NULL;
    if( !m_clusteringAlgorithmName.empty() ){
        std::cout << "run clusteringalgorithm" << std::endl;
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::RunClusteringAlgorithm(*this, m_clusteringAlgorithmName, pClusterList));
    }
    if( !m_inputClusterListName.empty() )
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::GetClusterList(*this, m_inputClusterListName, pClusterList));
    if( m_clusteringAlgorithmName.empty() && 
        m_inputClusterListName.empty() )
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::GetCurrentClusterList( *this, pClusterList ) );



    double energySum = 0.;
    CartesianVector momentumSum(0, 0, 0);

 //   std::cout << "pclusterlist size " << pClusterList->size() << std::endl;
    for (ClusterList::const_iterator itCluster = pClusterList->begin(), itClusterEnd = pClusterList->end(); itCluster != itClusterEnd; itCluster++)
    {
        CartesianVector momentum(0, 0, 0);
        float  mass = 0.0;
        int    particleId = 211;
        int    charge = 0;
        float  energy = 0.0;

        // --- energy
        if (m_energyFrom == "MC")
        {
            ComputeFromMc( (*itCluster), energy, momentum, mass, particleId, charge );
        }
        else if (m_energyFrom == "calorimeter")
        {
            ComputeFromCalorimeter( (*itCluster), energy, momentum, mass, particleId, charge );
        }
        else if (m_energyFrom == "tracks")
        {
            ComputeFromTracks( (*itCluster), energy, momentum, mass, particleId, charge );
        }
        else if (m_energyFrom == "calorimeterAndTracks")
        {
            ComputeFromCalorimeterAndTracks( (*itCluster), energy, momentum, mass, particleId, charge );
        }
        else
        {
            return STATUS_CODE_INVALID_PARAMETER;
        }

        // ----- create particle flow object
        PandoraContentApi::ParticleFlowObject::Parameters pfo;
        pfo.m_clusterList.insert((*itCluster)); // insert cluster into pfo

        //       insert tracks into pfo
        TrackList trackList = (*itCluster)->GetAssociatedTrackList();
        for (TrackList::iterator itTrack = trackList.begin(), itTrackEnd = trackList.end(); itTrack != itTrackEnd; ++itTrack)
        {
            pfo.m_trackList.insert((*itTrack));
        }

        //       set energy, charge, mass, momentum, particleId
        pfo.m_energy     = energy;
        pfo.m_charge     = charge;
        pfo.m_mass       = mass;
        pfo.m_momentum   = momentum;
        pfo.m_particleId = particleId;
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::ParticleFlowObject::Create(*this, pfo));
        // ------

        energySum += energy;
        momentumSum = momentumSum + momentum;
    }

   double pt = std::sqrt(momentumSum.GetX() * momentumSum.GetX() + momentumSum.GetY() * momentumSum.GetY());
   if( m_debug ) std::cout << "energySum " << energySum << "  pt " << pt << std::endl;

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode CheatingPfoCreationAlgorithm::ReadSettings(const TiXmlHandle xmlHandle)
{
//     settings example:
//
//     <algorithm type = "Cheating">
//         <algorithm type = "PerfectClustering" description = "Clustering">
//         </algorithm>
//         <energyFrom> tracks </energyFrom>
//         <clusterListName> CheatedClusterList </clusterListName>
//     </algorithm> 
//
    try
    {
        XmlHelper::ProcessFirstAlgorithm(*this, xmlHandle, m_clusteringAlgorithmName);
    }
    catch (StatusCodeException &statusCodeException)
    {
        m_clusteringAlgorithmName = "";
    }
    catch(...)
    {
        std::cout << "unknown exception in CheatingPfoCreationAlgorithm. (ReadSetting/ProcessFirstAlgorithm)" << std::endl;
        return STATUS_CODE_FAILURE;
    }

    try
    {
        XmlHelper::ReadValue(xmlHandle, "inputClusterListName", m_inputClusterListName);
    }
    catch (StatusCodeException &statusCodeException)
    {
        m_inputClusterListName    = "";
    }    
    catch(...)
    {
        std::cout << "unknown exception in CheatingPfoCreationAlgorithm. (ReadSetting/ReadValue/m_inputClusterListName)" << std::endl;
        return STATUS_CODE_FAILURE;
    }

    PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ReadValue(xmlHandle, "energyFrom", m_energyFrom));

    try
    {
        XmlHelper::ReadValue(xmlHandle, "debug", m_debug);
    }
    catch (StatusCodeException &statusCodeException)
    {
        m_debug   = false;
    }    
    catch(...)
    {
        std::cout << "unknown exception in CheatingPfoCreationAlgorithm. (ReadSetting/ReadValue/m_debug)" << std::endl;
        return STATUS_CODE_FAILURE;
    }

    std::cout << "clustering algorithm : " << m_clusteringAlgorithmName << std::endl;


    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

void CheatingPfoCreationAlgorithm::ComputeEnergyWeightedClusterPosition( Cluster* cluster, CartesianVector& energyWeightedClusterPosition )
{
    energyWeightedClusterPosition.SetValues( 0, 0, 0 ); // assign 0.0 for x, y and z position

    float energySum = 0.0;

    OrderedCaloHitList pOrderedCaloHitList = cluster->GetOrderedCaloHitList();
    for( OrderedCaloHitList::const_iterator itLyr = pOrderedCaloHitList.begin(), itLyrEnd = pOrderedCaloHitList.end(); itLyr != itLyrEnd; itLyr++ )
    {
        CaloHitList::iterator itCaloHit    = itLyr->second->begin();
        CaloHitList::iterator itCaloHitEnd = itLyr->second->end();

        for( ; itCaloHit != itCaloHitEnd; itCaloHit++ )
        {
            float hitEnergy = (*itCaloHit)->GetElectromagneticEnergy(); 
            energySum += hitEnergy;
            
            energyWeightedClusterPosition += (*itCaloHit)->GetPositionVector() * hitEnergy;
        }
    }
    energyWeightedClusterPosition *= 1.0/energySum;
}

//------------------------------------------------------------------------------------------------------------------------------------------

void CheatingPfoCreationAlgorithm::ComputeFromCalorimeter( pandora::Cluster* cluster, float& energy, CartesianVector& momentum, float& mass,
    int& particleId, int& charge )
{
    TrackList trackList = cluster->GetAssociatedTrackList();
    if (trackList.empty()) // cluster doesn't have tracks
    {
        if (cluster->IsPhoton())
        {
            energy = cluster->GetElectromagneticEnergy();
            momentum = cluster->GetFitToAllHitsResult().GetDirection();
            momentum = momentum * energy;
            particleId = 22; // assume it's a photon
            mass = 0.0;
            charge = 0;
        }
        else
        {
            energy = cluster->GetHadronicEnergy();
            particleId = 2112; // assume it's a neutron
            mass = 0.9396;
            charge = 0;

            CartesianVector energyWeightedClusterPosition(0.0,0.0,0.0);
            ComputeEnergyWeightedClusterPosition( cluster, energyWeightedClusterPosition );
                    
            CartesianVector clusterMomentum = energyWeightedClusterPosition * cluster->GetHadronicEnergy();
            const float totalGravity(energyWeightedClusterPosition.GetMagnitude());
                    
            momentum = clusterMomentum* (1.0/totalGravity);
        }
    }
    else // has tracks
    {
        // energy
        energy = cluster->GetHadronicEnergy();
        particleId = 211; // assume it's a pion
        mass = 0.1396;    // -- " --

        charge = 1;

        CartesianVector energyWeightedClusterPosition(0,0,0);
        ComputeEnergyWeightedClusterPosition( cluster, energyWeightedClusterPosition );
                    
        CartesianVector clusterMomentum = energyWeightedClusterPosition * cluster->GetHadronicEnergy();
        const float totalGravity(energyWeightedClusterPosition.GetMagnitude());
                    
        momentum = clusterMomentum* (1.0/totalGravity);
    }
    if( m_debug ) std::cout << "energy from calo: " << energy << std::endl;
}

//------------------------------------------------------------------------------------------------------------------------------------------

void CheatingPfoCreationAlgorithm::ComputeFromMc( pandora::Cluster* cluster, float& energy, CartesianVector& momentum, float& mass,
    int& particleId, int& charge )
{
    // match calohitvectors to their MCParticles
    std::map< MCParticle*, float > energyPerMCParticle;
    std::map< MCParticle*, float >::iterator itEnergyPerMCParticle;

    const OrderedCaloHitList pOrderedCaloHitList = cluster->GetOrderedCaloHitList();

    for( OrderedCaloHitList::const_iterator itLyr = pOrderedCaloHitList.begin(), itLyrEnd = pOrderedCaloHitList.end(); itLyr != itLyrEnd; itLyr++ )
    {
        // int pseudoLayer = itLyr->first;
        CaloHitList::iterator itCaloHit    = itLyr->second->begin();
        CaloHitList::iterator itCaloHitEnd = itLyr->second->end();

        for( ; itCaloHit != itCaloHitEnd; itCaloHit++ )
        {
            MCParticle* mc = NULL; 
            (*itCaloHit)->GetMCParticle( mc );
            if( mc == NULL ) continue; // has to be continue, since sometimes some CalorimeterHits don't have a MCParticle (e.g. noise)

            float energy = (*itCaloHit)->GetInputEnergy();

            itEnergyPerMCParticle = energyPerMCParticle.find( mc );
            if( itEnergyPerMCParticle == energyPerMCParticle.end() )
            {
                energyPerMCParticle.insert( std::make_pair( mc, energy ) );
            }
            else
            {
                itEnergyPerMCParticle->second += energy;
            }
        }
    }

    std::map<MCParticle*, float>::iterator it = max_element( energyPerMCParticle.begin(), energyPerMCParticle.end(), 
         pandora::Select2nd<std::map<MCParticle*, float>::value_type, std::greater<float> >() );

    MCParticle* mc = it->first;
    energy     = mc->GetEnergy();
    particleId = mc->GetParticleId();
}

//------------------------------------------------------------------------------------------------------------------------------------------

void CheatingPfoCreationAlgorithm::ComputeFromTracks( pandora::Cluster* cluster, float& energy, CartesianVector& momentum, float& mass,
    int& particleId, int& charge )
{
//     int num = 0;
    TrackList trackList = cluster->GetAssociatedTrackList();
    for (TrackList::iterator itTrack = trackList.begin(), itTrackEnd = trackList.end(); itTrack != itTrackEnd; ++itTrack)
    {
        mass += (*itTrack)->GetMass();
        energy += (*itTrack)->GetEnergyAtDca();
        particleId = 211;
//                charge <==  TODO get charge from track

        momentum = momentum + (*itTrack)->GetMomentumAtDca();

//                MCParticle *mc = NULL;
//                (*itTrack)->GetMCParticle( mc );
//                std::cout << "track number " << num << " energy " << std::sqrt(momentum*momentum+mass*mass) << "  mc->energy " << mc->GetEnergy() << " mc->momentum " << mc->GetMomentum() << " momentum " << momentum << std::endl;
//         ++num;
    }
    if( m_debug ) std::cout << "energy from tracks " << energy << std::endl;
}

//------------------------------------------------------------------------------------------------------------------------------------------

void CheatingPfoCreationAlgorithm::ComputeFromCalorimeterAndTracks( pandora::Cluster* cluster, float& energy, CartesianVector& momentum,
    float& mass, int& particleId, int& charge )
{
    TrackList trackList = cluster->GetAssociatedTrackList();
    if (trackList.empty()) // cluster doesn't have tracks
    {
        ComputeFromCalorimeter( cluster, energy, momentum, mass, particleId, charge );
    }
    else
    {
        ComputeFromTracks( cluster, energy, momentum, mass, particleId, charge );
    }
}
