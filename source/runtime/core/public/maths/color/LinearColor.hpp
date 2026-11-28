// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/ContainerForward.hpp"
#include "CoreMinimal.hpp"
#include "maths/base/Arithmetic.hpp"
#include "maths/base/Constants.hpp"
#include "maths/base/FloatingPoint.hpp"   // IWYU pragma: keep
#include "maths/base/Interpolation.hpp"   // IWYU pragma: keep
#include "maths/color/Color.hpp"
#include "maths/color/ColorForward.hpp"   // IWYU pragma: keep
#include "maths/MathForward.hpp"
#include "maths/vector/Vector3.hpp"
#include "maths/vector/Vector4.hpp"
#include <type_traits>

namespace gp::math
{

/// @brief A linear, 32-bit floating-point per-component RGBA color (`16 bytes`).
/// @details Mirrors Unreal Engine's `FLinearColor`. Designed for linear-light shading, HDR rendering, color-space
///          transformations, HSV interpolation, and black-body temperature conversions.
struct LinearColor final
{
public:
    /// @brief Red channel in linear space (`0.0f` = black, `1.0f` = reference white, `> 1.0f` = HDR).
    gp::Float32 r{ 0.0f };

    /// @brief Green channel in linear space.
    gp::Float32 g{ 0.0f };

    /// @brief Blue channel in linear space.
    gp::Float32 b{ 0.0f };

    /// @brief Alpha / opacity channel in linear `[0.0f, 1.0f]`.
    gp::Float32 a{ 0.0f };

public:
    // clang-format off
    /// @brief Static 256-entry lookup table for fast `Color` (sRGB) -> `LinearColor` decoding.
    /// @details `c > 0.04045 ? pow(c * (1.0 / 1.055) + 0.0521327, 2.4) : c * (1.0 / 12.92)` for `c = i / 255.0`.
    static constexpr gp::Float32 kSRGBToLinearTable[256] =
    {
        0.0f,
        0.000303526983548838f, 0.000607053967097675f, 0.000910580950646512f, 0.00121410793419535f, 0.00151763491774419f,
        0.00182116190129302f, 0.00212468888484186f, 0.0024282158683907f, 0.00273174285193954f, 0.00303526983548838f,
        0.00334653564113713f, 0.00367650719436314f, 0.00402471688178252f, 0.00439144189356217f, 0.00477695332960869f,
        0.005181516543916f, 0.00560539145834456f, 0.00604883284946662f, 0.00651209061157708f, 0.00699540999852809f,
        0.00749903184667767f, 0.00802319278093555f, 0.0085681254056307f, 0.00913405848170623f, 0.00972121709156193f,
        0.0103298227927056f, 0.0109600937612386f, 0.0116122449260844f, 0.012286488094766f, 0.0129830320714536f,
        0.0137020827679224f, 0.0144438433080002f, 0.0152085141260192f, 0.0159962930597398f, 0.0168073754381669f,
        0.0176419541646397f, 0.0185002197955389f, 0.0193823606149269f, 0.0202885627054049f, 0.0212190100154473f,
        0.0221738844234532f, 0.02315336579873f, 0.0241576320596103f, 0.0251868592288862f, 0.0262412214867272f,
        0.0273208912212394f, 0.0284260390768075f, 0.0295568340003534f, 0.0307134432856324f, 0.0318960326156814f,
        0.0331047661035236f, 0.0343398063312275f, 0.0356013143874111f, 0.0368894499032755f, 0.0382043710872463f,
        0.0395462347582974f, 0.0409151963780232f, 0.0423114100815264f, 0.0437350287071788f, 0.0451862038253117f,
        0.0466650857658898f, 0.0481718236452158f, 0.049706565391714f, 0.0512694577708345f, 0.0528606464091205f,
        0.0544802758174765f, 0.0561284894136735f, 0.0578054295441256f, 0.0595112375049707f, 0.0612460535624849f,
        0.0630100169728596f, 0.0648032660013696f, 0.0666259379409563f, 0.0684781691302512f, 0.070360094971063f,
        0.0722718499453493f, 0.0742135676316953f, 0.0761853807213167f, 0.0781874210336082f, 0.0802198195312533f,
        0.0822827063349132f, 0.0843762107375113f, 0.0865004612181274f, 0.0886555854555171f, 0.0908417103412699f,
        0.0930589619926197f, 0.0953074657649191f, 0.0975873462637915f, 0.0998987273569704f, 0.102241732185838f,
        0.104616483176675f, 0.107023102051626f, 0.109461709839399f, 0.1119324268857f, 0.114435372863418f,
        0.116970666782559f, 0.119538426999953f, 0.122138771228724f, 0.124771816547542f, 0.127437679409664f,
        0.130136475651761f, 0.132868320502552f, 0.135633328591233f, 0.138431613955729f, 0.141263290050755f,
        0.144128469755705f, 0.147027265382362f, 0.149959788682454f, 0.152926150855031f, 0.155926462553701f,
        0.158960833893705f, 0.162029374458845f, 0.16513219330827f, 0.168269398983119f, 0.171441099513036f,
        0.174647402422543f, 0.17788841473729f, 0.181164242990184f, 0.184474993227387f, 0.187820771014205f,
        0.191201681440861f, 0.194617829128147f, 0.198069318232982f, 0.201556252453853f, 0.205078735036156f,
        0.208636868777438f, 0.212230756032542f, 0.215860498718652f, 0.219526198320249f, 0.223227955893977f,
        0.226965872073417f, 0.23074004707378f, 0.23455058069651f, 0.238397572333811f, 0.242281120973093f,
        0.246201325201334f, 0.250158283209375f, 0.254152092796134f, 0.258182851372752f, 0.262250655966664f,
        0.266355603225604f, 0.270497789421545f, 0.274677310454565f, 0.278894261856656f, 0.283148738795466f,
        0.287440836077983f, 0.291770648154158f, 0.296138269120463f, 0.300543792723403f, 0.304987312362961f,
        0.309468921095997f, 0.313988711639584f, 0.3185467763743f, 0.323143207347467f, 0.32777809627633f,
        0.332451534551205f, 0.337163613238559f, 0.341914423084057f, 0.346704054515559f, 0.351532597646068f,
        0.356400142276637f, 0.361306777899234f, 0.36625259369956f, 0.371237678559833f, 0.376262121061519f,
        0.381326009488037f, 0.386429431827418f, 0.39157247577492f, 0.396755228735618f, 0.401977777826949f,
        0.407240209881218f, 0.41254261144808f, 0.417885068796976f, 0.423267667919539f, 0.428690494531971f,
        0.434153634077377f, 0.439657171728079f, 0.445201192387887f, 0.450785780694349f, 0.456411021020965f,
        0.462076997479369f, 0.467783793921492f, 0.473531493941681f, 0.479320180878805f, 0.485149937818323f,
        0.491020847594331f, 0.496932992791578f, 0.502886455747457f, 0.50888131855397f, 0.514917663059676f,
        0.520995570871595f, 0.527115123357109f, 0.533276401645826f, 0.539479486631421f, 0.545724458973463f,
        0.552011399099209f, 0.558340387205378f, 0.56471150325991f, 0.571124827003694f, 0.577580437952282f,
        0.584078415397575f, 0.590618838409497f, 0.597201785837643f, 0.603827336312907f, 0.610495568249093f,
        0.617206559844509f, 0.623960389083534f, 0.630757133738175f, 0.637596871369601f, 0.644479679329661f,
        0.651405634762384f, 0.658374814605461f, 0.665387295591707f, 0.672443154250516f, 0.679542466909286f,
        0.686685309694841f, 0.693871758534824f, 0.701101889159085f, 0.708375777101046f, 0.71569349769906f,
        0.723055126097739f, 0.730460737249286f, 0.737910405914797f, 0.745404206665559f, 0.752942213884326f,
        0.760524501766589f, 0.768151144321824f, 0.775822215374732f, 0.783537788566466f, 0.791297937355839f,
        0.799102735020525f, 0.806952254658248f, 0.81484656918795f, 0.822785751350956f, 0.830769873712124f,
        0.838799008660978f, 0.846873228412837f, 0.854992605009927f, 0.863157210322481f, 0.871367116049835f,
        0.879622393721502f, 0.887923114698241f, 0.896269350173118f, 0.904661171172551f, 0.913098648557343f,
        0.921581853023715f, 0.930110855104312f, 0.938685725169219f, 0.947306533426946f, 0.955973349925421f,
        0.964686244552961f, 0.973445287039244f, 0.982250546956257f, 0.991102093719252f, 1.0f
    };

    /// @brief Static 256-entry lookup table for fast `Color` (gamma 2.2) -> `LinearColor` decoding (`pow(i / 255.0, 2.2)`).
    static constexpr gp::Float32 kPow22OneOver255Table[256] =
    {
        0.0f, 5.07705190066176E-06f, 2.33280046660989E-05f, 5.69217657121931E-05f, 0.000107187362341244f, 0.000175123977503027f, 0.000261543754548491f, 0.000367136269815943f, 0.000492503787191433f,
        0.000638182842167022f, 0.000804658499513058f, 0.000992374304074325f, 0.0012017395224384f, 0.00143313458967186f, 0.00168691531678928f, 0.00196341621339647f, 0.00226295316070643f,
        0.00258582559623417f, 0.00293231832393836f, 0.00330270303200364f, 0.00369723957890013f, 0.00411617709328275f, 0.00455975492252602f, 0.00502820345685554f, 0.00552174485023966f,
        0.00604059365484981f, 0.00658495738258168f, 0.00715503700457303f, 0.00775102739766061f, 0.00837311774514858f, 0.00902149189801213f, 0.00969632870165823f, 0.0103978022925553f,
        0.0111260823683832f, 0.0118813344348137f, 0.0126637200315821f, 0.0134733969401426f, 0.0143105193748841f, 0.0151752381596252f, 0.0160677008908869f, 0.01698805208925f, 0.0179364333399502f,
        0.0189129834237215f, 0.0199178384387857f, 0.0209511319147811f, 0.0220129949193365f, 0.0231035561579214f, 0.0242229420675342f, 0.0253712769047346f, 0.0265486828284729f, 0.027755279978126f,
        0.0289911865471078f, 0.0302565188523887f, 0.0315513914002264f, 0.0328759169483838f, 0.034230206565082f, 0.0356143696849188f, 0.0370285141619602f, 0.0384727463201946f, 0.0399471710015256f,
        0.0414518916114625f, 0.0429870101626571f, 0.0445526273164214f, 0.0461488424223509f, 0.0477757535561706f, 0.049433457555908f, 0.0511220500564934f, 0.052841625522879f, 0.0545922772817603f,
        0.0563740975519798f, 0.0581871774736854f, 0.0600316071363132f, 0.0619074756054558f, 0.0638148709486772f, 0.0657538802603301f, 0.0677245896854243f, 0.0697270844425988f, 0.0717614488462391f,
        0.0738277663277846f, 0.0759261194562648f, 0.0780565899581019f, 0.080219258736215f, 0.0824142058884592f, 0.0846415107254295f, 0.0869012517876603f, 0.0891935068622478f, 0.0915183529989195f,
        0.0938758665255778f, 0.0962661230633397f, 0.0986891975410945f, 0.1011451642096f, 0.103634096655137f, 0.106156067812744f, 0.108711149979039f, 0.11129941482466f, 0.113920933406333f,
        0.116575776178572f, 0.119264013005047f, 0.121985713169619f, 0.124740945387051f, 0.127529777813422f, 0.130352278056244f, 0.1332085131843f, 0.136098549737202f, 0.139022453734703f,
        0.141980290685736f, 0.144972125597231f, 0.147998022982685f, 0.151058046870511f, 0.154152260812165f, 0.157280727890073f, 0.160443510725344f, 0.16364067148529f, 0.166872271890766f,
        0.170138373223312f, 0.173439036332135f, 0.176774321640903f, 0.18014428915439f, 0.183548998464951f, 0.186988508758844f, 0.190462878822409f, 0.193972167048093f, 0.19751643144034f,
        0.201095729621346f, 0.204710118836677f, 0.208359655960767f, 0.212044397502288f, 0.215764399609395f, 0.219519718074868f, 0.223310408341127f, 0.227136525505149f, 0.230998124323267f,
        0.23489525921588f, 0.238827984272048f, 0.242796353254002f, 0.24680041960155f, 0.2508402364364f, 0.254915856566385f, 0.259027332489606f, 0.263174716398492f, 0.267358060183772f,
        0.271577415438375f, 0.275832833461245f, 0.280124365261085f, 0.284452061560024f, 0.288815972797219f, 0.293216149132375f, 0.297652640449211f, 0.302125496358853f, 0.306634766203158f,
        0.311180499057984f, 0.315762743736397f, 0.32038154879181f, 0.325036962521076f, 0.329729032967515f, 0.334457807923889f, 0.339223334935327f, 0.344025661302187f, 0.348864834082879f,
        0.353740900096629f, 0.358653905926199f, 0.363603897920553f, 0.368590922197487f, 0.373615024646202f, 0.37867625092984f, 0.383774646487975f, 0.388910256539059f, 0.394083126082829f,
        0.399293299902674f, 0.404540822567962f, 0.409825738436323f, 0.415148091655907f, 0.420507926167587f, 0.425905285707146f, 0.43134021380741f, 0.436812753800359f, 0.442322948819202f,
        0.44787084180041f, 0.453456475485731f, 0.45907989242416f, 0.46474113497389f, 0.470440245304218f, 0.47617726539744f, 0.481952237050698f, 0.487765201877811f, 0.493616201311074f,
        0.49950527660303f, 0.505432468828216f, 0.511397818884879f, 0.517401367496673f, 0.523443155214325f, 0.529523222417277f, 0.535641609315311f, 0.541798355950137f, 0.547993502196972f,
        0.554227087766085f, 0.560499152204328f, 0.566809734896638f, 0.573158875067523f, 0.579546611782525f, 0.585972983949661f, 0.592438030320847f, 0.598941789493296f, 0.605484299910907f,
        0.612065599865624f, 0.61868572749878f, 0.625344720802427f, 0.632042617620641f, 0.638779455650817f, 0.645555272444934f, 0.652370105410821f, 0.659223991813387f, 0.666116968775851f,
        0.673049073280942f, 0.680020342172095f, 0.687030812154625f, 0.694080519796882f, 0.701169501531402f, 0.708297793656032f, 0.715465432335048f, 0.722672453600255f, 0.729918893352071f,
        0.737204787360605f, 0.744530171266715f, 0.751895080583051f, 0.759299550695091f, 0.766743616862161f, 0.774227314218442f, 0.781750677773962f, 0.789313742415586f, 0.796916542907978f,
        0.804559113894567f, 0.81224148989849f, 0.819963705323528f, 0.827725794455034f, 0.835527791460841f, 0.843369730392169f, 0.851251645184515f, 0.859173569658532f, 0.867135537520905f,
        0.875137582365205f, 0.883179737672745f, 0.891262036813419f, 0.899384513046529f, 0.907547199521614f, 0.915750129279253f, 0.923993335251873f, 0.932276850264543f, 0.940600707035753f,
        0.948964938178195f, 0.957369576199527f, 0.96581465350313f, 0.974300202388861f, 0.982826255053791f, 0.99139284359294f, 1.0f
    };
    // clang-format on

public:
    /// @section Constructors

    /// @brief Default constructor. Initializes all components to zero (`(0.0f, 0.0f, 0.0f, 0.0f)`).
    [[nodiscard]] constexpr LinearColor() noexcept = default;

    /// @brief Constructs a linear color from explicit Red, Green, Blue, and optional Alpha components.
    /// @param[in] inR Red component.
    /// @param[in] inG Green component.
    /// @param[in] inB Blue component.
    /// @param[in] inA Alpha component (defaults to `1.0f` opaque).
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr LinearColor(
        const gp::Float32 inR, const gp::Float32 inG, const gp::Float32 inB, const gp::Float32 inA = 1.0f
    ) noexcept
        : r(inR)
        , g(inG)
        , b(inB)
        , a(inA)
    {}

    /// @brief Converts an 8-bit `Color` (assumed to be in sRGB space) into linear color space via `kSRGBToLinearTable`.
    /// @note To dequantize without sRGB gamma expansion, use `Color::reinterpretAsLinear()`.
    /// @param[in] color Source 8-bit sRGB color.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr LinearColor(const Color& color) noexcept
        : r(kSRGBToLinearTable[color.r])
        , g(kSRGBToLinearTable[color.g])
        , b(kSRGBToLinearTable[color.b])
        , a(static_cast<gp::Float32>(color.a) * (1.0f / 255.0f))
    {}

    /// @brief Constructs a linear color from a single-precision 3D vector `(x, y, z)` with `a = 1.0f`.
    /// @param[in] vector Source 3D vector.
    [[nodiscard]] constexpr LinearColor(const Vector3<gp::Float32>& vector) noexcept
        : r(vector.x)
        , g(vector.y)
        , b(vector.z)
        , a(1.0f)
    {}

    /// @brief Explicitly constructs a linear color from a double-precision 3D vector `(x, y, z)` with `a = 1.0f`.
    /// @param[in] vector Source 3D double vector.
    [[nodiscard]] explicit constexpr LinearColor(const Vector3<gp::Float64>& vector) noexcept
        : r(static_cast<gp::Float32>(vector.x))
        , g(static_cast<gp::Float32>(vector.y))
        , b(static_cast<gp::Float32>(vector.z))
        , a(1.0f)
    {}

    /// @brief Constructs a linear color from a single-precision 4D vector `(x, y, z, w)`.
    /// @param[in] vector Source 4D vector.
    [[nodiscard]] constexpr LinearColor(const Vector4<gp::Float32>& vector) noexcept
        : r(vector.x)
        , g(vector.y)
        , b(vector.z)
        , a(vector.w)
    {}

    /// @brief Explicitly constructs a linear color from a double-precision 4D vector `(x, y, z, w)`.
    /// @param[in] vector Source 4D double vector.
    [[nodiscard]] explicit constexpr LinearColor(const Vector4<gp::Float64>& vector) noexcept
        : r(static_cast<gp::Float32>(vector.x))
        , g(static_cast<gp::Float32>(vector.y))
        , b(static_cast<gp::Float32>(vector.z))
        , a(static_cast<gp::Float32>(vector.w))
    {}

public:
    /// @section Component Access & Operators

    /// @brief Accesses a mutable color channel by index (`0` = R, `1` = G, `2` = B, `3` = A).
    /// @param[in] index Channel index in `[0, 3]`.
    /// @return Mutable reference to the selected component.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::Float32& component(const gp::Int32 index) noexcept
    {
        GP_ASSERT(index >= 0 && index < 4, "LinearColor::component index out of [0, 3] bounds.");
        switch (index)
        {
        case 0:
            return r;
        case 1:
            return g;
        case 2:
            return b;
        default:
            return a;
        }
    }

    /// @brief Accesses a const color channel by index (`0` = R, `1` = G, `2` = B, `3` = A).
    /// @param[in] index Channel index in `[0, 3]`.
    /// @return Const reference to the selected component.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr const gp::Float32& component(const gp::Int32 index) const noexcept
    {
        GP_ASSERT(index >= 0 && index < 4, "LinearColor::component const index out of [0, 3] bounds.");
        switch (index)
        {
        case 0:
            return r;
        case 1:
            return g;
        case 2:
            return b;
        default:
            return a;
        }
    }

    /// @brief Subscript operator for mutable channel access (`0` = R, `1` = G, `2` = B, `3` = A).
    /// @param[in] index Channel index in `[0, 3]`.
    /// @return Mutable reference to the selected component.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::Float32& operator[](const gp::Int32 index) noexcept
    {
        return component(index);
    }

    /// @brief Subscript operator for const channel access (`0` = R, `1` = G, `2` = B, `3` = A).
    /// @param[in] index Channel index in `[0, 3]`.
    /// @return Const reference to the selected component.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr const gp::Float32& operator[](const gp::Int32 index) const noexcept
    {
        return component(index);
    }

    /// @brief Component-wise addition of two linear colors.
    /// @param[in] other Color to add.
    /// @return Summed `LinearColor`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr LinearColor operator+(const LinearColor& other) const noexcept
    {
        return LinearColor(r + other.r, g + other.g, b + other.b, a + other.a);
    }

    /// @brief In-place component-wise addition.
    /// @param[in] other Color to add.
    /// @return Reference to `*this`.
    GP_FORCEINLINE_HINT constexpr LinearColor& operator+=(const LinearColor& other) noexcept
    {
        r += other.r;
        g += other.g;
        b += other.b;
        a += other.a;
        return *this;
    }

    /// @brief Component-wise subtraction of two linear colors.
    /// @param[in] other Color to subtract.
    /// @return Difference `LinearColor`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr LinearColor operator-(const LinearColor& other) const noexcept
    {
        return LinearColor(r - other.r, g - other.g, b - other.b, a - other.a);
    }

    /// @brief In-place component-wise subtraction.
    /// @param[in] other Color to subtract.
    /// @return Reference to `*this`.
    GP_FORCEINLINE_HINT constexpr LinearColor& operator-=(const LinearColor& other) noexcept
    {
        r -= other.r;
        g -= other.g;
        b -= other.b;
        a -= other.a;
        return *this;
    }

    /// @brief Component-wise multiplication (modulation) of two linear colors.
    /// @param[in] other Color to multiply by.
    /// @return Modulated `LinearColor`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr LinearColor operator*(const LinearColor& other) const noexcept
    {
        return LinearColor(r * other.r, g * other.g, b * other.b, a * other.a);
    }

    /// @brief In-place component-wise multiplication.
    /// @param[in] other Color to multiply by.
    /// @return Reference to `*this`.
    GP_FORCEINLINE_HINT constexpr LinearColor& operator*=(const LinearColor& other) noexcept
    {
        r *= other.r;
        g *= other.g;
        b *= other.b;
        a *= other.a;
        return *this;
    }

    /// @brief Multiplies all four channels by a scalar factor.
    /// @param[in] scalar Scale factor.
    /// @return Scaled `LinearColor`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr LinearColor operator*(const gp::Float32 scalar) const noexcept
    {
        return LinearColor(r * scalar, g * scalar, b * scalar, a * scalar);
    }

    /// @brief In-place scalar multiplication.
    /// @param[in] scalar Scale factor.
    /// @return Reference to `*this`.
    GP_FORCEINLINE_HINT constexpr LinearColor& operator*=(const gp::Float32 scalar) noexcept
    {
        r *= scalar;
        g *= scalar;
        b *= scalar;
        a *= scalar;
        return *this;
    }

    /// @brief Component-wise division of two linear colors.
    /// @param[in] other Divisor color.
    /// @return Quotient `LinearColor`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr LinearColor operator/(const LinearColor& other) const noexcept
    {
        return LinearColor(r / other.r, g / other.g, b / other.b, a / other.a);
    }

    /// @brief In-place component-wise division.
    /// @param[in] other Divisor color.
    /// @return Reference to `*this`.
    GP_FORCEINLINE_HINT constexpr LinearColor& operator/=(const LinearColor& other) noexcept
    {
        r /= other.r;
        g /= other.g;
        b /= other.b;
        a /= other.a;
        return *this;
    }

    /// @brief Divides all four channels by a scalar factor.
    /// @param[in] scalar Non-zero scalar divisor.
    /// @return Scaled `LinearColor`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr LinearColor operator/(const gp::Float32 scalar) const noexcept
    {
        const gp::Float32 invScalar = 1.0f / scalar;
        return LinearColor(r * invScalar, g * invScalar, b * invScalar, a * invScalar);
    }

    /// @brief In-place scalar division.
    /// @param[in] scalar Non-zero scalar divisor.
    /// @return Reference to `*this`.
    GP_FORCEINLINE_HINT constexpr LinearColor& operator/=(const gp::Float32 scalar) noexcept
    {
        const gp::Float32 invScalar = 1.0f / scalar;
        r *= invScalar;
        g *= invScalar;
        b *= invScalar;
        a *= invScalar;
        return *this;
    }

    /// @brief Exact component-wise equality comparison.
    /// @param[in] other Color to compare against.
    /// @return True if all four components are strictly equal.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool operator==(const LinearColor& other) const noexcept
    {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }

    /// @brief Exact component-wise inequality comparison.
    /// @param[in] other Color to compare against.
    /// @return True if any component differs.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool operator!=(const LinearColor& other) const noexcept
    {
        return r != other.r || g != other.g || b != other.b || a != other.a;
    }

public:
    /// @section Clamping, Comparisons & Channel Queries

    /// @brief Returns a copy of this color with every channel clamped to `[inMin, inMax]`.
    /// @param[in] inMin Lower bound (defaults to `0.0f`).
    /// @param[in] inMax Upper bound (defaults to `1.0f`).
    /// @return Clamped `LinearColor`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr LinearColor
        getClamped(const gp::Float32 inMin = 0.0f, const gp::Float32 inMax = 1.0f) const noexcept
    {
        return LinearColor(
            gp::math::clamp(r, inMin, inMax),
            gp::math::clamp(g, inMin, inMax),
            gp::math::clamp(b, inMin, inMax),
            gp::math::clamp(a, inMin, inMax)
        );
    }

    /// @brief Tolerance-based component-wise comparison.
    /// @param[in] other Color to compare against.
    /// @param[in] tolerance Maximum allowed absolute difference per channel (defaults to `kindaSmallNumber`).
    /// @return True if `|c_a - c_b| < tolerance` for all four channels.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool equals(
        const LinearColor& other, const gp::Float32 tolerance = Constants<gp::Float32>::kindaSmallNumber
    ) const noexcept
    {
        return gp::math::abs(r - other.r) < tolerance && gp::math::abs(g - other.g) < tolerance &&
               gp::math::abs(b - other.b) < tolerance && gp::math::abs(a - other.a) < tolerance;
    }

    /// @brief Returns a copy of this linear color with its opacity (`a`) replaced by @p newOpacity.
    /// @param[in] newOpacity New alpha value.
    /// @return `LinearColor(r, g, b, newOpacity)`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr LinearColor
        copyWithNewOpacity(const gp::Float32 newOpacity) const noexcept
    {
        return LinearColor(r, g, b, newOpacity);
    }

    /// @brief Clamps a float to `[0.0f, 1.0f]`, mapping NaNs and negatives to `0.0f` for GPU conversion parity.
    /// @param[in] inValue Input scalar.
    /// @return Clamped value in `[0.0f, 1.0f]`.
    [[nodiscard]] GP_FORCEINLINE_HINT static constexpr gp::Float32 clamp01NansTo0(const gp::Float32 inValue) noexcept
    {
        return Color::clamp01NansTo0(inValue);
    }

    /// @brief Returns the maximum value across all four `(R, G, B, A)` channels.
    /// @return Maximum channel value.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::Float32 getMax() const noexcept
    {
        return gp::math::max(gp::math::max(gp::math::max(r, g), b), a);
    }

    /// @brief Returns the minimum value across all four `(R, G, B, A)` channels.
    /// @return Minimum channel value.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::Float32 getMin() const noexcept
    {
        return gp::math::min(gp::math::min(gp::math::min(r, g), b), a);
    }

    /// @brief Checks whether the RGB channels have negligible energy (`r^2 < delta && g^2 < delta && b^2 < delta`).
    /// @details Useful for culling lights or emissive contributions that will not visibly affect the frame.
    /// @return True if `r^2`, `g^2`, and `b^2` are each strictly less than `Constants<Float32>::delta` (`1e-5f`).
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isAlmostBlack() const noexcept
    {
        constexpr gp::Float32 kDelta = Constants<gp::Float32>::delta;
        return (r * r) < kDelta && (g * g) < kDelta && (b * b) < kDelta;
    }

    /// @brief Computes the 4D Euclidean distance between two linear colors.
    /// @param[in] v1 First linear color.
    /// @param[in] v2 Second linear color.
    /// @return `sqrt((v2.r - v1.r)^2 + (v2.g - v1.g)^2 + (v2.b - v1.b)^2 + (v2.a - v1.a)^2)`.
    [[nodiscard]] GP_FORCEINLINE_HINT static constexpr gp::Float32
        dist(const LinearColor& v1, const LinearColor& v2) noexcept
    {
        const gp::Float32 dr = v2.r - v1.r;
        const gp::Float32 dg = v2.g - v1.g;
        const gp::Float32 db = v2.b - v1.b;
        const gp::Float32 da = v2.a - v1.a;
        return gp::math::sqrt(dr * dr + dg * dg + db * db + da * da);
    }

    /// @brief Converts this linear color to a 3D vector `(r, g, b)` discarding alpha.
    /// @return `Vector3<Float32>(r, g, b)`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Vector3<gp::Float32> toVector3() const noexcept
    {
        return Vector3<gp::Float32>(r, g, b);
    }

    /// @brief Converts this linear color to a 4D vector `(r, g, b, a)`.
    /// @return `Vector4<Float32>(r, g, b, a)`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Vector4<gp::Float32> toVector4() const noexcept
    {
        return Vector4<gp::Float32>(r, g, b, a);
    }

public:
    /// @section Color Space, Gamma, RGBE & HSV Conversions

    /// @brief Converts an 8-bit sRGB `Color` into a `LinearColor`.
    /// @param[in] color Source 8-bit sRGB color.
    /// @return Decoded `LinearColor`.
    [[nodiscard]] GP_FORCEINLINE_HINT static constexpr LinearColor fromSRGBColor(const Color& color) noexcept
    {
        return LinearColor(color);
    }

    /// @brief Converts an 8-bit gamma-2.2 `Color` into a `LinearColor` via `kPow22OneOver255Table`.
    /// @param[in] color Source 8-bit Pow(1/2.2) color.
    /// @return Decoded `LinearColor`.
    [[nodiscard]] GP_FORCEINLINE_HINT static constexpr LinearColor fromPow22Color(const Color& color) noexcept
    {
        return LinearColor(
            kPow22OneOver255Table[color.r],
            kPow22OneOver255Table[color.g],
            kPow22OneOver255Table[color.b],
            static_cast<gp::Float32>(color.a) * (1.0f / 255.0f)
        );
    }

    /// @brief Encodes this linear HDR color into an 8-bit Radiance RGBE (`8:8:8:8` shared-exponent) `Color`.
    /// @details Negative channels are clamped to `0.0f`; values `< 1e-32f` map to `Color(0, 0, 0, 0)`.
    /// @return Shared-exponent RGBE `Color`.
    [[nodiscard]] GP_CORE_API Color toRGBE() const noexcept;

    /// @brief Converts a byte-encoded HSV triple (`H, S, V` in `[0, 255]`) to a linear RGB color.
    /// @details `H` is mapped across `[0, 360)` via `H * (360.0f / 256.0f)`, `S` and `V` via `/ 255.0f`.
    /// @param[in] h Byte hue (`[0, 255]`).
    /// @param[in] s Byte saturation (`[0, 255]`).
    /// @param[in] v Byte value / brightness (`[0, 255]`).
    /// @return Linear RGB `LinearColor` with `a = 1.0f`.
    [[nodiscard]] GP_CORE_API static LinearColor makeFromHSV8(gp::UInt8 h, gp::UInt8 s, gp::UInt8 v) noexcept;

    /// @brief Converts this linear RGB color into HSV representation (`r = H [0, 360)`, `g = S [0, 1]`, `b = V`,
    ///        `a = a`).
    /// @return HSV `LinearColor`.
    [[nodiscard]] GP_CORE_API LinearColor linearRGBToHSV() const noexcept;

    /// @brief Converts this HSV color (`r = H`, `g = S`, `b = V`, `a = a`) into linear RGB space.
    /// @return Linear RGB `LinearColor`.
    [[nodiscard]] GP_CORE_API LinearColor hsvToLinearRGB() const noexcept;

    /// @brief Linearly interpolates between two linear RGB colors in HSV space taking the shortest angular hue path.
    /// @param[in] from Start linear RGBA color.
    /// @param[in] to End linear RGBA color.
    /// @param[in] progress Interpolation factor (typically `[0.0f, 1.0f]`).
    /// @return Interpolated linear RGBA color.
    [[nodiscard]] GP_CORE_API static LinearColor
        lerpUsingHSV(const LinearColor& from, const LinearColor& to, gp::Float32 progress) noexcept;

    /// @brief Quantizes this linear color directly to an 8-bit `Color` with `+0.5f` rounding (bypasses sRGB encoding).
    /// @details Matches the GPU `FLOAT -> UNORM8` specification and pairs with `Color::reinterpretAsLinear()`.
    /// @return Rounded 8-bit `Color`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Color quantizeRound() const noexcept
    {
        return Color(
            static_cast<gp::UInt8>(0.5f + clamp01NansTo0(r) * 255.0f),
            static_cast<gp::UInt8>(0.5f + clamp01NansTo0(g) * 255.0f),
            static_cast<gp::UInt8>(0.5f + clamp01NansTo0(b) * 255.0f),
            static_cast<gp::UInt8>(0.5f + clamp01NansTo0(a) * 255.0f)
        );
    }

    /// @brief Quantizes this linear color directly to an 8-bit `Color` using floor truncation (bypasses sRGB encoding).
    /// @return Truncated 8-bit `Color`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Color quantizeFloor() const noexcept
    {
        return Color(
            static_cast<gp::UInt8>(clamp01NansTo0(r) * 255.0f),
            static_cast<gp::UInt8>(clamp01NansTo0(g) * 255.0f),
            static_cast<gp::UInt8>(clamp01NansTo0(b) * 255.0f),
            static_cast<gp::UInt8>(clamp01NansTo0(a) * 255.0f)
        );
    }

    /// @brief Quantizes this linear color to an 8-bit sRGB `Color` using the fast IEEE-754 mantissa table / SSE2 path.
    /// @details Guarantees exact round-trip identity (`LinearColor(c).toColorSRGB() == c` for all `c in [0, 255]`).
    ///          NaNs and negatives map to `0`; values `>= 1.0f` map to `255`.
    /// @return sRGB-encoded 8-bit `Color`.
    [[nodiscard]] GP_CORE_API Color toColorSRGB() const noexcept;

    /// @brief Alias for `toColorSRGB()` preserving Unreal `ToFColorSRGB()` naming familiarity.
    /// @return sRGB-encoded 8-bit `Color`.
    [[nodiscard]] GP_FORCEINLINE_HINT Color toFColorSRGB() const noexcept
    {
        return toColorSRGB();
    }

    /// @brief Quantizes this linear color to an 8-bit `Color` with optional sRGB gamma encoding.
    /// @param[in] srgb When `true`, applies sRGB OETF (`toColorSRGB()`); when `false`, uses `quantizeRound()`.
    /// @return 8-bit `Color`.
    [[nodiscard]] GP_FORCEINLINE_HINT Color toColor(const bool srgb) const noexcept
    {
        return srgb ? toColorSRGB() : quantizeRound();
    }

    /// @brief Alias for `toColor(srgb)` preserving Unreal `ToFColor()` naming familiarity.
    /// @param[in] srgb When `true`, applies sRGB OETF; when `false`, uses `quantizeRound()`.
    /// @return 8-bit `Color`.
    [[nodiscard]] GP_FORCEINLINE_HINT Color toFColor(const bool srgb) const noexcept
    {
        return toColor(srgb);
    }

    /// @brief Computes the perceptually weighted luminance of this color in the active working `ColorSpace`.
    /// @details Uses the working color space's CIE Y luminance factors (or `(0.3f, 0.59f, 0.11f)` when the console
    ///          variable `r.LegacyLuminanceFactors` is non-zero).
    /// @return Perceptual luminance scalar.
    [[nodiscard]] GP_CORE_API gp::Float32 getLuminance() const noexcept;

    /// @brief Computes the perceptual luminance using explicit RGB weight factors.
    /// @param[in] luminanceFactors Custom RGB luminance weights.
    /// @return `r * luminanceFactors.r + g * luminanceFactors.g + b * luminanceFactors.b`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::Float32
        getLuminance(const LinearColor& luminanceFactors) const noexcept
    {
        return r * luminanceFactors.r + g * luminanceFactors.g + b * luminanceFactors.b;
    }

    /// @brief Linearly interpolates this color toward its monochromatic luminance `(lum, lum, lum, 0.0f)`.
    /// @param[in] desaturation Desaturation factor (`0.0f` = original color, `1.0f` = full grayscale).
    /// @return Desaturated `LinearColor`.
    [[nodiscard]] GP_CORE_API LinearColor desaturate(gp::Float32 desaturation) const noexcept;

public:
    /// @section Procedural Generation, Splines & Formatting

    /// @brief Generates a visually pleasing random color with full saturation and value.
    /// @return Random opaque `LinearColor`.
    [[nodiscard]] GP_CORE_API static LinearColor makeRandomColor() noexcept;

    /// @brief Generates a deterministic random color in `[0, 1]^3` from an explicit 32-bit integer seed.
    /// @param[in] seed Deterministic seed value.
    /// @return Seeded `LinearColor` with `a = 1.0f`.
    [[nodiscard]] GP_CORE_API static LinearColor makeRandomSeededColor(gp::Int32 seed) noexcept;

    /// @brief Converts a black-body radiator temperature in Kelvins (`[1000K, 15000K]`) to linear RGB chromaticity.
    /// @details Approximates the Planckian locus in CIE 1960 UCS, converts to CIE 1931 XYZ, and projects to BT.709 RGB.
    /// @param[in] temp Black-body temperature in Kelvins (clamped to `[1000.0f, 15000.0f]`).
    /// @return Linear RGB `LinearColor` with `a = 1.0f`.
    [[nodiscard]] GP_CORE_API static LinearColor makeFromColorTemperature(gp::Float32 temp) noexcept;

    /// @brief Generates a distinct color from an integer index by stepping around the hue wheel by the golden ratio.
    /// @param[in] seed Sequence index / seed.
    /// @param[in] saturation Saturation in `[0.0f, 1.0f]` (defaults to `1.0f`).
    /// @param[in] value Brightness in `[0.0f, 1.0f]` (defaults to `1.0f`).
    /// @param[in] hueRotation Base angular step in degrees (defaults to `180.0f`).
    /// @return Distinct `LinearColor`.
    [[nodiscard]] static LinearColor intToDistinctColor(
        const gp::Int32 seed,
        const gp::Float32 saturation = 1.0f,
        const gp::Float32 value = 1.0f,
        const gp::Float32 hueRotation = 180.0f
    ) noexcept
    {
        constexpr gp::Float32 kGoldenRatio = 1.618033988749895f;
        return LinearColor(static_cast<gp::Float32>(seed) * kGoldenRatio * hueRotation, saturation, value)
            .hsvToLinearRGB();
    }

    /// @brief Evaluates a cubic Bezier curve defined by 4 `LinearColor` control points into @p outPoints.
    /// @param[in] controlPoints Pointer to an array of 4 `LinearColor` control points.
    /// @param[in] numPoints Number of sample points to evaluate (`>= 2`).
    /// @param[out] outPoints Vector receiving the `numPoints` evaluated colors.
    /// @return Cumulative chord-length path distance along the sampled curve.
    [[nodiscard]] GP_CORE_API static gp::Float32 evaluateBezier(
        const LinearColor* controlPoints, gp::Int32 numPoints, gp::Vector<LinearColor>& outPoints
    ) noexcept;

    /// @brief Formats this linear color as `"(R=%f,G=%f,B=%f,A=%f)"` with 6 fixed decimal places.
    /// @return Formatted `gp::String`.
    [[nodiscard]] GP_CORE_API gp::String toString() const noexcept;

    /// @brief Initializes this linear color from a string containing `"R=..."`, `"G=..."`, `"B=..."`, and optional
    ///        `"A=..."`.
    /// @details Defaults `a` to `1.0f` when `"A="` is omitted. Returns `false` if any of `R=`, `G=`, or `B=` is missing
    ///          or unparseable.
    /// @param[in] sourceString Input string view to parse.
    /// @return True if `R`, `G`, and `B` were successfully parsed.
    GP_CORE_API bool initFromString(gp::StringView sourceString) noexcept;

public:
    /// @section Hashing Support

    /// @brief Streams all four 32-bit float channels into any `gp::hashing` accumulator.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    template <typename InHasher>
    constexpr void hashAppend(InHasher& hasher) const noexcept
    {
        hasher.append(r);
        hasher.append(g);
        hasher.append(b);
        hasher.append(a);
    }

public:
    /// @section Named LinearColor Constants

    static const LinearColor kWhite;
    static const LinearColor kGray;
    static const LinearColor kBlack;
    static const LinearColor kTransparent;
    static const LinearColor kRed;
    static const LinearColor kGreen;
    static const LinearColor kBlue;
    static const LinearColor kYellow;
};

static_assert(sizeof(LinearColor) == 16u, "gp::math::LinearColor must be exactly 16 bytes.");
static_assert(alignof(LinearColor) == 4u, "gp::math::LinearColor must have 4-byte alignment.");
static_assert(std::is_trivially_copyable_v<LinearColor>, "gp::math::LinearColor must be trivially copyable.");
static_assert(std::is_standard_layout_v<LinearColor>, "gp::math::LinearColor must have standard layout.");

inline constexpr LinearColor LinearColor::kWhite(1.0f, 1.0f, 1.0f, 1.0f);
inline constexpr LinearColor LinearColor::kGray(0.5f, 0.5f, 0.5f, 1.0f);
inline constexpr LinearColor LinearColor::kBlack(0.0f, 0.0f, 0.0f, 1.0f);
inline constexpr LinearColor LinearColor::kTransparent(0.0f, 0.0f, 0.0f, 0.0f);
inline constexpr LinearColor LinearColor::kRed(1.0f, 0.0f, 0.0f, 1.0f);
inline constexpr LinearColor LinearColor::kGreen(0.0f, 1.0f, 0.0f, 1.0f);
inline constexpr LinearColor LinearColor::kBlue(0.0f, 0.0f, 1.0f, 1.0f);
inline constexpr LinearColor LinearColor::kYellow(1.0f, 1.0f, 0.0f, 1.0f);

/// @brief Multiplies a `LinearColor` by a scalar factor on the left-hand side (`scalar * color`).
/// @param[in] scalar Scale factor.
/// @param[in] color Input linear color.
/// @return Scaled `LinearColor`.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr LinearColor
    operator*(const gp::Float32 scalar, const LinearColor& color) noexcept
{
    return color * scalar;
}

/// @brief Linearly interpolates between two `LinearColor` values component-wise.
/// @tparam InAlphaT Floating-point type of the interpolation factor.
/// @param[in] a Start color (`t == 0`).
/// @param[in] b End color (`t == 1`).
/// @param[in] t Interpolation factor.
/// @return Component-wise linearly interpolated `LinearColor`.
template <concepts::IsFloatingPoint InAlphaT>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr LinearColor
    lerp(const LinearColor& a, const LinearColor& b, const InAlphaT t) noexcept
{
    const gp::Float32 alpha = static_cast<gp::Float32>(t);
    return LinearColor(
        a.r + alpha * (b.r - a.r), a.g + alpha * (b.g - a.g), a.b + alpha * (b.b - a.b), a.a + alpha * (b.a - a.a)
    );
}

// Deferred inline definition of Color::reinterpretAsLinear() now that LinearColor is complete.
constexpr LinearColor Color::reinterpretAsLinear() const noexcept
{
    constexpr gp::Float32 kInv255 = 1.0f / 255.0f;
    return LinearColor(
        static_cast<gp::Float32>(r) * kInv255,
        static_cast<gp::Float32>(g) * kInv255,
        static_cast<gp::Float32>(b) * kInv255,
        static_cast<gp::Float32>(a) * kInv255
    );
}

/// @brief Decomposes a linear HDR color into a normalized 8-bit sRGB `Color` and a scalar intensity multiplier.
/// @param[in] inLinearColor Input linear color.
/// @param[out] outColor Receives `(inLinearColor / outIntensity).toColor(true)`.
/// @param[out] outIntensity Receives `max(delta, max(r, g, b))`.
GP_CORE_API void computeAndFixedColorAndIntensity(
    const LinearColor& inLinearColor, Color& outColor, gp::Float32& outIntensity
) noexcept;

/// @brief Batch-converts an array of `LinearColor` values to sRGB-encoded 8-bit `Color` values.
/// @param[in] inLinearColors Source array of `inCount` linear colors.
/// @param[out] outColorsSRGB Destination array of `inCount` sRGB colors.
/// @param[in] inCount Number of elements to convert (`<= 0` is a safe no-op).
GP_CORE_API void
    convertLinearColorsToColorSRGB(const LinearColor* inLinearColors, Color* outColorsSRGB, gp::Int64 inCount) noexcept;

}   // namespace gp::math
