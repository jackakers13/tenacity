/**********************************************************************

  Audacity: A Digital Audio Editor

  Echo.cpp

  Dominic Mazzoni
  Vaughan Johnson (dialog)

*******************************************************************//**

\class EffectEcho
\brief An Effect that causes an echo, variable delay and volume.

*//****************************************************************//**

\class EchoDialog
\brief EchoDialog used with EffectEcho

*//*******************************************************************/


#include "Echo.h"
#include "LoadEffects.h"

#include <cfloat>

#include <wx/intl.h>

#include "../shuttle/ShuttleGui.h"
#include "../shuttle/Shuttle.h"
#include "../widgets/AudacityMessageBox.h"
#include "../widgets/valnum.h"

// Define keys, defaults, minimums, and maximums for the effect parameters
//
//     Name    Type     Key            Def   Min      Max      Scale
Param( Delay,  float,   wxT("Delay"),   1.0f, 0.001f,  FLT_MAX, 1.0f );
Param( Decay,  float,   wxT("Decay"),   0.5f, 0.0f,    FLT_MAX, 1.0f );

const ComponentInterfaceSymbol EffectEcho::Symbol
{ XO("Echo") };

namespace{ BuiltinEffectsModule::Registration< EffectEcho > reg; }

EffectEcho::EffectEcho()
{
   InstanceInit(mMainState, mSampleRate);
   SetLinearEffectFlag(true);
}

EffectEcho::~EffectEcho()
{
}

// ComponentInterface implementation

ComponentInterfaceSymbol EffectEcho::GetSymbol()
{
   return Symbol;
}

TranslatableString EffectEcho::GetDescription()
{
   return XO("Repeats the selected audio again and again");
}

ManualPageID EffectEcho::ManualPage()
{
   return L"Echo";
}

// EffectDefinitionInterface implementation

EffectType EffectEcho::GetType()
{
   return EffectTypeProcess;
}

// EffectClientInterface implementation

unsigned EffectEcho::GetAudioInCount()
{
   return 1;
}

unsigned EffectEcho::GetAudioOutCount()
{
   return 1;
}

bool EffectEcho::ProcessInitialize(sampleCount WXUNUSED(totalLen), ChannelNames WXUNUSED(chanMap))
{
   InstanceInit(mMainState, mSampleRate);
   if (mMainState.delay == 0.0)
   {
      return false;
   }

   auto requestedHistLen = (sampleCount) (mSampleRate * mMainState.delay);

   // Guard against extreme delay values input by the user
   try {
      // Guard against huge delay values from the user.
      // Don't violate the assertion in as_size_t
      if (requestedHistLen !=
            (mMainState.histLen = static_cast<size_t>(requestedHistLen.as_long_long())))
         throw std::bad_alloc{};
      mMainState.history.reinit(mMainState.histLen, true);
   }
   catch ( const std::bad_alloc& ) {
      Effect::MessageBox( XO("Requested value exceeds memory capacity.") );
      return false;
   }

   return mMainState.history != NULL;
}

bool EffectEcho::ProcessFinalize()
{
   mMainState.history.reset();
   return true;
}

size_t EffectEcho::ProcessBlock(float **inBlock, float **outBlock, size_t blockLen)
{
   float *ibuf = inBlock[0];
   float *obuf = outBlock[0];

   for (size_t i = 0; i < blockLen; i++, mMainState.histPos++)
   {
      if (mMainState.histPos == mMainState.histLen)
      {
         mMainState.histPos = 0;
      }

      mMainState.history[mMainState.histPos] = obuf[i] = ibuf[i] + mMainState.history[mMainState.histPos] * mMainState.decay;
   }

   return blockLen;
}

bool EffectEcho::DefineParams( ShuttleParams & S ){
   S.SHUTTLE_PARAM( mMainState.delay, Delay );
   S.SHUTTLE_PARAM( mMainState.decay, Decay );
   return true;
}


bool EffectEcho::GetAutomationParameters(CommandParameters & parms)
{
   parms.WriteFloat(KEY_Delay, mMainState.delay);
   parms.WriteFloat(KEY_Decay, mMainState.decay);

   return true;
}

bool EffectEcho::SetAutomationParameters(CommandParameters & parms)
{
   ReadAndVerifyFloat(Delay);
   ReadAndVerifyFloat(Decay);

   mMainState.delay = Delay;
   mMainState.decay = Decay;

   return true;
}

void EffectEcho::PopulateOrExchange(ShuttleGui & S)
{
   S.AddSpace(0, 5);

   S.StartMultiColumn(2, wxALIGN_CENTER);
   {
      S.Validator<FloatingPointValidator<double>>(
            3, &mMainState.delay, NumValidatorStyle::NO_TRAILING_ZEROES,
            MIN_Delay, MAX_Delay
         )
         .AddTextBox(XXO("&Delay time (seconds):"), wxT(""), 10);

      S.Validator<FloatingPointValidator<double>>(
            3, &mMainState.decay, NumValidatorStyle::NO_TRAILING_ZEROES,
            MIN_Decay, MAX_Decay)
         .AddTextBox(XXO("D&ecay factor:"), wxT(""), 10);
   }
   S.EndMultiColumn();
}

bool EffectEcho::TransferDataToWindow()
{
   if (!mUIParent->TransferDataToWindow())
   {
      return false;
   }

   return true;
}

bool EffectEcho::TransferDataFromWindow()
{
   if (!mUIParent->Validate() || !mUIParent->TransferDataFromWindow())
   {
      return false;
   }

   return true;
}

void EffectEcho::InstanceInit(EffectEchoState& state, double sampleRate)
{
   state.sampleRate = sampleRate;
   state.delay      = DEF_Delay;
   state.decay      = DEF_Decay;
   state.history.reset();
   state.histPos    = 0;
   state.histLen    = 0;
}
