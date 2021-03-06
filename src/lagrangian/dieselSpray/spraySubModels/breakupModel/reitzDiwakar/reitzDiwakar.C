/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2011 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

#include "error.H"

#include "reitzDiwakar.H"
#include "addToRunTimeSelectionTable.H"
#include "basicMultiComponentMixture.H"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
    defineTypeNameAndDebug(reitzDiwakar, 0);

    addToRunTimeSelectionTable
    (
        breakupModel,
        reitzDiwakar,
        dictionary
    );
}

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::reitzDiwakar::reitzDiwakar
(
    const dictionary& dict,
    spray& sm
)
:
    breakupModel(dict, sm),
    coeffsDict_(dict.subDict(typeName + "Coeffs")),
    Cbag_(readScalar(coeffsDict_.lookup("Cbag"))),
    Cb_(readScalar(coeffsDict_.lookup("Cb"))),
    Cstrip_(readScalar(coeffsDict_.lookup("Cstrip"))),
    Cs_(readScalar(coeffsDict_.lookup("Cs")))
{}


// * * * * * * * * * * * * * * * * Destructor  * * * * * * * * * * * * * * * //

Foam::reitzDiwakar::~reitzDiwakar()
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

void Foam::reitzDiwakar::breakupParcel
(
    parcel& p,
    const scalar deltaT,
    const vector& vel,
    const liquidMixtureProperties& fuels
) const
{
    const PtrList<volScalarField>& Y = spray_.composition().Y();

    label cellI = p.cell();
    scalar pressure = spray_.p()[cellI];
    scalar temperature = spray_.T()[cellI];
    scalar Taverage = p.T() + (temperature - p.T())/3.0;

    scalar muAverage = 0.0;
    scalar Winv = 0.0;
    forAll(Y, i)
    {
        Winv += Y[i][cellI]/spray_.gasProperties()[i].W();
        muAverage += Y[i][cellI]*spray_.gasProperties()[i].mu(Taverage);
    }
    scalar R = specie::RR*Winv;

    // ideal gas law to evaluate density
    scalar rhoAverage = pressure/R/Taverage;
    scalar nuAverage = muAverage/rhoAverage;
    scalar sigma = fuels.sigma(pressure, p.T(), p.X());


    // The We and Re numbers are to be evaluated using the 1/3 rule.

    scalar WeberNumber = p.We(vel, rhoAverage, sigma);
    scalar ReynoldsNumber = p.Re(vel, nuAverage);

    scalar sqRey = sqrt(ReynoldsNumber);

    if (WeberNumber > Cbag_)
    {
        if (WeberNumber > Cstrip_*sqRey)
        {
            scalar dStrip =
                sqr(2.0*Cstrip_*sigma)
               /(
                    rhoAverage
                   *pow3(mag(p.Urel(vel)))
                   *muAverage
                );

            scalar tauStrip =
                Cs_*p.d()
               *sqrt(fuels.rho(pressure, p.T(), p.X())/rhoAverage)
               /mag(p.Urel(vel));

            scalar fraction = deltaT/tauStrip;

            // new droplet diameter, implicit calculation
            p.d() = (fraction*dStrip + p.d())/(1.0 + fraction);
        }
        else
        {
            scalar dBag = 2.0*Cbag_*sigma/(rhoAverage*sqr(mag(p.Urel(vel))));

            scalar tauBag =
                Cb_*p.d()*sqrt(fuels.rho(pressure, p.T(), p.X())*p.d()/sigma);

            scalar fraction = deltaT/tauBag;

            // new droplet diameter, implicit calculation
            p.d() = (fraction*dBag + p.d())/(1.0 + fraction);
        }
    }
}


// ************************************************************************* //
