#include <DD4hep/DetFactoryHelper.h>
#include "DD4hep/DetType.h"
#include <XML/Layering.h>
#include "XML/Utilities.h"
#include "DDRec/DetectorData.h"

#include <string>
using namespace DD4hep;
using namespace DD4hep::Geometry;


static DD4hep::Geometry::Ref_t create_detector(DD4hep::Geometry::LCDD& lcdd,
					       xml_h element,
					       DD4hep::Geometry::SensitiveDetector sens) {

  std::cout << __PRETTY_FUNCTION__  << std::endl;
  std::cout << "Here is my LHCal_v01"  << std::endl;
  std::cout << " and this is the sensitive detector: " << &sens  << std::endl;
  sens.setType("calorimeter");
  //Materials
  DD4hep::Geometry::Material air = lcdd.air();

  //Access to the XML File
  xml_det_t     xmlLHCal     = element;
  std::string   detName  = xmlLHCal.nameStr();


  DD4hep::Geometry::DetElement sdet ( detName,  xmlLHCal.id() );


  // --- create an envelope volume and position it into the world ---------------------
  
  DD4hep::Geometry::Volume envelope = DD4hep::XML::createPlacedEnvelope( lcdd,  element , sdet ) ;
  
  sdet.setTypeFlag( DetType::CALORIMETER |  DetType::ENDCAP  | DetType::HADRONIC |  DetType::FORWARD ) ;

  if( lcdd.buildType() == DD4hep::BUILD_ENVELOPE ) return sdet ;
  
  //-----------------------------------------------------------------------------------
  //Parameters we have to know about
  DD4hep::XML::Component xmlParameter = xmlLHCal.child(_Unicode(parameter));
  const double mradFullCrossingAngle  = xmlParameter.attr< double >(_Unicode(crossingangle));
 
  DD4hep::XML::Dimension dimensions =  xmlLHCal.dimensions();

  //LHCal Dimensions
  const double lhcalInnerR = dimensions.inner_r();
  const double lhcalwidth = dimensions.width();
  const double lhcalheight = dimensions.height();
  const double lhcalInnerZ = dimensions.inner_z();
  const double lhcalThickness = DD4hep::Layering(xmlLHCal).totalThickness();
  const double lhcalCentreZ = lhcalInnerZ+lhcalThickness*0.5;
  //  const double lhcaloffset = dimensions.offset();
  const double lhcaloffset = std::tan( mradFullCrossingAngle/2. )*lhcalCentreZ;

  std::cout << " Building detector: " << detName << std::endl;
  std::cout << " - Main crossing angle: " << mradFullCrossingAngle << " radian"  << std::endl;
  std::cout << " - LHCal begin Z         "    << lhcalInnerZ/dd4hep::cm  << " cm" << std::endl;
  std::cout << " - LHCal center Z        "    << lhcalCentreZ/dd4hep::cm  << " cm" << std::endl;
  std::cout << " - LHCal offset X        "    << lhcaloffset/dd4hep::cm  << " cm" << std::endl;
  std::cout << " - LHCal inner R         "    << lhcalInnerR/dd4hep::cm  << " cm" << std::endl;
  std::cout << " - LHCal thickness       "    << lhcalThickness/dd4hep::cm  << " cm" << std::endl;

  double LHcal_cell_size      = lcdd.constant<double>("LHcal_cell_size");
  double LHCal_outer_radius   = lcdd.constant<double>("LHCal_outer_radius");
  //========== fill data for reconstruction ============================
  DD4hep::DDRec::LayeredCalorimeterData* caloData = new DD4hep::DDRec::LayeredCalorimeterData ;
  caloData->layoutType = DD4hep::DDRec::LayeredCalorimeterData::EndcapLayout ;
  caloData->inner_symmetry = 0  ; // hard code cernter pipe hole
  caloData->outer_symmetry = 4  ; // outer box
  caloData->phi0 = 0 ;

  /// extent of the calorimeter in the r-z-plane [ rmin, rmax, zmin, zmax ] in mm.
  caloData->extent[0] = lhcalInnerR ;
  caloData->extent[1] = LHCal_outer_radius ;
  caloData->extent[2] = lhcalInnerZ ;
  caloData->extent[3] = lhcalInnerZ + lhcalThickness ;

  // counter for the current layer to be placed
  int thisLayerId = 1;

  //Envelope to place the layers in
  DD4hep::Geometry::Box envelopeBox (lhcalwidth*0.5, lhcalheight*0.5, lhcalThickness*0.5 );

  // octagon to make inner bore in LHcal
  int nSides = 8;
  double phiStart = 22.5*dd4hep::deg;
  double rMax = lhcalInnerR/std::cos( phiStart );
  DD4hep::Geometry::PolyhedraRegular innerBore( nSides, phiStart, 0., rMax, lhcalThickness ); 
  DD4hep::Geometry::SubtractionSolid LHCalModule (envelopeBox, innerBore );
  
  DD4hep::Geometry::Volume     envelopeVol(detName+"_module",LHCalModule,air);
  envelopeVol.setVisAttributes(lcdd,xmlLHCal.visStr());


  ////////////////////////////////////////////////////////////////////////////////
  // Create all the layers
  ////////////////////////////////////////////////////////////////////////////////

  //Loop over all the layer (repeat=NN) sections
  //This is the starting point to place all layers, we need this when we have more than one layer block
  double referencePosition = -lhcalThickness*0.5;
  for(DD4hep::XML::Collection_t coll(xmlLHCal,_U(layer)); coll; ++coll)  {
    DD4hep::XML::Component xmlLayer(coll); //we know this thing is a layer


    //This just calculates the total size of a single layer
    double layerThickness = 0.;
    for(DD4hep::XML::Collection_t l(xmlLayer,_U(slice)); l; ++l) layerThickness += xml_comp_t(l).thickness();

    std::cout << " - Layer Thickness " << layerThickness/dd4hep::cm << " cm" << std::endl;
    std::cout << " - Total Length "    << lhcalThickness/dd4hep::cm  << " cm" << std::endl;

    //Loop for repeat=NN
    for(int i=0, repeat=xmlLayer.repeat(); i<repeat; ++i)  {

      std::string layer_name = detName + DD4hep::XML::_toString(thisLayerId,"_layer%d");
      DD4hep::Geometry::Box layer_base(lhcalwidth*0.5, lhcalheight*0.5,layerThickness*0.5);
      DD4hep::Geometry::SubtractionSolid layer_subtracted;
     layer_subtracted = DD4hep::Geometry::SubtractionSolid(layer_base, innerBore );
     

      DD4hep::Geometry::Volume layer_vol(layer_name,layer_subtracted,air);


      int sliceID=1;
      double inThisLayerPosition = -layerThickness*0.5;

      double radiator_thickness = 0.0;

      DD4hep::DDRec::LayeredCalorimeterData::Layer caloLayer ;
      caloLayer.cellSize0 = LHcal_cell_size ;
      caloLayer.cellSize1 = LHcal_cell_size ;

      double nRadiationLengths=0.;
      double nInteractionLengths=0.;
      double thickness_sum=0;

      for(DD4hep::XML::Collection_t collSlice(xmlLayer,_U(slice)); collSlice; ++collSlice)  {
	DD4hep::XML::Component compSlice = collSlice;
	const double      sliceThickness = compSlice.thickness();
	const std::string sliceName = layer_name + DD4hep::XML::_toString(sliceID,"slice%d");
	DD4hep::Geometry::Material   slice_mat  = lcdd.material(compSlice.materialStr());

	DD4hep::Geometry::Box sliceBase(lhcalwidth*0.5, lhcalheight*0.5,sliceThickness*0.5);
	DD4hep::Geometry::SubtractionSolid slice_subtracted;

	slice_subtracted = DD4hep::Geometry::SubtractionSolid(sliceBase, innerBore );

	DD4hep::Geometry::Volume slice_vol (sliceName,slice_subtracted,slice_mat);

	if ( compSlice.materialStr().compare("TungstenDens24") == 0 ) radiator_thickness = sliceThickness;

	nRadiationLengths   += sliceThickness/(2.*slice_mat.radLength());
	nInteractionLengths += sliceThickness/(2.*slice_mat.intLength());
	thickness_sum       += sliceThickness/2.;

	if ( compSlice.isSensitive() )  {
	  slice_vol.setSensitiveDetector(sens);

#if DD4HEP_VERSION_GE( 0, 15 )
	  //Store "inner" quantities
	  caloLayer.inner_nRadiationLengths   = nRadiationLengths;
	  caloLayer.inner_nInteractionLengths = nInteractionLengths;
	  caloLayer.inner_thickness           = thickness_sum;
	  //Store scintillator thickness
	  caloLayer.sensitive_thickness       = sliceThickness;
#endif
	  //Reset counters to measure "outside" quantitites
	  nRadiationLengths   = 0.;
	  nInteractionLengths = 0.;
	  thickness_sum       = 0.;
	}

	nRadiationLengths   += sliceThickness/(2.*slice_mat.radLength());
	nInteractionLengths += sliceThickness/(2.*slice_mat.intLength());
	thickness_sum       += sliceThickness/2.;

	slice_vol.setAttributes(lcdd,compSlice.regionStr(),compSlice.limitsStr(),compSlice.visStr());
	DD4hep::Geometry::PlacedVolume pv = layer_vol.placeVolume(slice_vol, DD4hep::Geometry::Position(0,0,inThisLayerPosition+sliceThickness*0.5));

	pv.addPhysVolID("slice",sliceID);
	inThisLayerPosition += sliceThickness;

	++sliceID;
      }//For all slices in this layer


#if DD4HEP_VERSION_GE( 0, 15 )
      //Store "outer" quantities
      caloLayer.outer_nRadiationLengths   = nRadiationLengths;
      caloLayer.outer_nInteractionLengths = nInteractionLengths;
      caloLayer.outer_thickness           = thickness_sum;
#endif
      //-----------------------------------------------------------------------------------------
      caloLayer.distance = lhcalCentreZ + referencePosition ; //+0.5*layerThickness ;
      caloLayer.absorberThickness = radiator_thickness ;
      
      caloData->layers.push_back( caloLayer ) ;
      //-----------------------------------------------------------------------------------------

      layer_vol.setVisAttributes(lcdd,xmlLayer.visStr());

      DD4hep::Geometry::Position layer_pos(0,0,referencePosition+0.5*layerThickness);
      referencePosition += layerThickness;
      DD4hep::Geometry::PlacedVolume pv = envelopeVol.placeVolume(layer_vol,layer_pos);
      pv.addPhysVolID("layer",thisLayerId);

      ++thisLayerId;

    }//for all layers

  }// for all layer collections

 
  const DD4hep::Geometry::Position bcForwardPos (lhcaloffset,0.0, lhcalCentreZ);
  const DD4hep::Geometry::Position bcBackwardPos(lhcaloffset,0.0,-lhcalCentreZ);
  const DD4hep::Geometry::RotationY bcForwardRot ( mradFullCrossingAngle/2. );
  const DD4hep::Geometry::RotationY bcBackwardRot( M_PI - mradFullCrossingAngle/2. );

  DD4hep::Geometry::PlacedVolume pv =
    envelope.placeVolume(envelopeVol, DD4hep::Geometry::Transform3D( bcForwardRot, bcForwardPos ) );
  pv.addPhysVolID("barrel", 1);

  DD4hep::Geometry::PlacedVolume pv2 =
    envelope.placeVolume(envelopeVol, DD4hep::Geometry::Transform3D( bcBackwardRot, bcBackwardPos ) );
  pv2.addPhysVolID("barrel", 2);

  sdet.addExtension< DD4hep::DDRec::LayeredCalorimeterData >( caloData ) ;

  return sdet;
}

DECLARE_DETELEMENT(LHCal_v01,create_detector)
