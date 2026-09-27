// Minimal declarations of the Unreal Engine 5 API used by Beaconhold, for a syntax and type
// check of the game module outside the engine (clang -fsyntax-only; see check.py).
//
// The file is cut into sections ("//@section Name: Required Sections"). check.py writes each
// section as its own header and maps every engine include to the sections it provides, so a
// file that uses a class without including its header fails the check (as it can in Unreal
// when the precompiled header does not happen to cover it).
//
// Only declarations: nothing here is linked or run. Signatures follow UE 5.x as closely as
// they matter for the calls the game makes (default arguments, constness, return types).
// A clean check means the code is consistent with these declarations; it does not prove the
// engine has exactly this API — that is verified by building in Unreal.
#pragma once

//@section Core
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <type_traits>
#include <utility>

// ============================================================================ Core types

using int8 = int8_t;
using uint8 = uint8_t;
using int16 = int16_t;
using uint16 = uint16_t;
using int32 = int32_t;
using uint32 = uint32_t;
using int64 = int64_t;
using uint64 = uint64_t;
using TCHAR = char16_t; // UTF-16 as on Android/iOS/Linux
using ANSICHAR = char;
using SIZE_T = size_t;

#define TEXT(x) u##x
#define FORCEINLINE inline
#define check(Expr) ((void)(Expr))
#define ensure(Expr) (!!(Expr))
#define PLATFORM_ANDROID 0
#define PLATFORM_IOS 0
//@section CoreUObject: Core
#define UCLASS(...)
#define USTRUCT(...)
#define UENUM(...)
#define UPROPERTY(...)
#define UFUNCTION(...)
#define UPARAM(...)
//@section SoundBase: CoreUObject EngineFwd
#define INDEFINITELY_LOOPING_DURATION 10000.0f

//@section Core
const TCHAR* StubUtf8ToTchar(const char* Utf8);
#define UTF8_TO_TCHAR(Str) StubUtf8ToTchar(Str)

template <typename T>
typename std::remove_reference<T>::type&& MoveTemp(T&& Obj);

// Stub bodies that must return a value use this; nothing here is ever run. Templates that
// take lambdas or file-local types need bodies, or clang rejects them as undefined.
void* StubAnyPtr();
template <typename T>
T StubMake()
{
	return *static_cast<T*>(StubAnyPtr());
}

namespace ELogVerbosity
{
enum Type : uint8
{
	NoLogging,
	Fatal,
	Error,
	Warning,
	Display,
	Log,
	Verbose,
	VeryVerbose,
	All
};
}
struct FLogCategoryBase
{
};
#define DECLARE_LOG_CATEGORY_EXTERN(Name, Default, Compile) extern FLogCategoryBase Name;
#define DEFINE_LOG_CATEGORY(Name) FLogCategoryBase Name;
// Printf-style format strings, checked at compile time against the argument types (the
// engine checks only that arguments are plain values; a mismatch misprints or crashes at run
// time). %d/%i/%u/%x/%c take 32-bit integers, %lld 64-bit, %f/%e/%g floating point, %s a
// TCHAR string (pass *String, never an FString) and %p a pointer.
void StubFormatError(const char* Message); // not constexpr: reaching it fails the check

template <typename T>
consteval char StubFormatKind()
{
	using D = std::decay_t<T>;
	if constexpr (std::is_same_v<D, const TCHAR*> || std::is_same_v<D, TCHAR*>)
		return 's';
	else if constexpr (std::is_pointer_v<D>)
		return 'p';
	else if constexpr (std::is_floating_point_v<D>)
		return 'f';
	else if constexpr ((std::is_integral_v<D> || std::is_enum_v<D>) && sizeof(D) == 8)
		return 'L';
	else if constexpr (std::is_integral_v<D> || std::is_enum_v<D>)
		return 'd';
	else
		return '?';
}

template <typename... Types>
struct TStubFormat
{
	template <size_t N>
	consteval TStubFormat(const char16_t (&Fmt)[N])
	{
		const char Kinds[] = {StubFormatKind<Types>()..., 0};
		size_t Arg = 0;
		for (size_t I = 0; I + 1 < N; ++I)
		{
			if (Fmt[I] != u'%')
				continue;
			++I;
			if (Fmt[I] == u'%')
				continue;
			while (Fmt[I] == u'-' || Fmt[I] == u'+' || Fmt[I] == u' ' || Fmt[I] == u'#' || Fmt[I] == u'0')
				++I;
			while (Fmt[I] >= u'0' && Fmt[I] <= u'9')
				++I;
			if (Fmt[I] == u'.')
			{
				++I;
				while (Fmt[I] >= u'0' && Fmt[I] <= u'9')
					++I;
			}
			bool bWide = false;
			if (Fmt[I] == u'l' && Fmt[I + 1] == u'l')
			{
				bWide = true;
				I += 2;
			}
			if (Arg >= sizeof...(Types))
				StubFormatError("format string wants more arguments than were passed");
			const char Kind = Kinds[Arg++];
			switch (Fmt[I])
			{
			case u'd':
			case u'i':
			case u'u':
			case u'x':
			case u'X':
			case u'c':
				if (Kind != (bWide ? 'L' : 'd'))
					StubFormatError("integer specifier does not match the argument (int32 for %d, int64 for %lld)");
				break;
			case u'f':
			case u'e':
			case u'g':
				if (Kind != 'f')
					StubFormatError("%f needs a float or double argument");
				break;
			case u's':
				if (Kind != 's')
					StubFormatError("%s needs a const TCHAR* argument (use *String)");
				break;
			case u'p':
				if (Kind != 'p' && Kind != 's')
					StubFormatError("%p needs a pointer argument");
				break;
			default:
				StubFormatError("unsupported format specifier");
			}
		}
		if (Arg != sizeof...(Types))
			StubFormatError("more arguments were passed than the format string uses");
	}
};

template <typename... Types>
void StubLog(const FLogCategoryBase& Category, ELogVerbosity::Type Verbosity, TStubFormat<std::type_identity_t<Types>...> Format, Types... Args);
#define UE_LOG(Category, Verbosity, Format, ...) StubLog(Category, ELogVerbosity::Verbosity, Format, ##__VA_ARGS__)

// ============================================================================ Containers

template <typename T>
class TArrayView
{
public:
	TArrayView();
	TArrayView(std::initializer_list<typename std::remove_const<T>::type> List);
	int32 Num() const;
};

template <typename T>
class TArray
{
public:
	TArray();
	TArray(std::initializer_list<T> List);
	TArray(const T* Ptr, int32 Count);
	TArray(const TArray& Other);
	TArray(TArray&& Other);
	TArray& operator=(const TArray& Other);
	TArray& operator=(TArray&& Other);

	int32 Num() const;
	bool IsEmpty() const;
	bool IsValidIndex(int32 Index) const;
	T& operator[](int32 Index);
	const T& operator[](int32 Index) const;
	T* GetData();
	const T* GetData() const;
	int32 Add(const T& Item);
	int32 Add(T&& Item);
	template <typename... ArgsType>
	int32 Emplace(ArgsType&&... Args);
	T& AddDefaulted_GetRef();
	void Reset(int32 NewSize = 0);
	void Empty(int32 Slack = 0);
	void SetNum(int32 NewNum, bool bAllowShrinking = true);
	void SetNumUninitialized(int32 NewNum, bool bAllowShrinking = true);
	void Reserve(int32 Number);
	void RemoveAt(int32 Index, int32 Count = 1, bool bAllowShrinking = true);
	void RemoveAtSwap(int32 Index, int32 Count = 1, bool bAllowShrinking = true);
	int32 RemoveSingleSwap(const T& Item, bool bAllowShrinking = true);
	T Pop(bool bAllowShrinking = true);
	bool Contains(const T& Item) const;
	T& Last(int32 IndexFromTheEnd = 0);
	T* begin();
	T* end();
	const T* begin() const;
	const T* end() const;
};

template <typename T>
class TIndirectArray
{
public:
	int32 Num() const;
	T& operator[](int32 Index);
	const T& operator[](int32 Index) const;
	int32 Add(T* Item);
};

template <typename KeyType, typename ValueType>
struct TPair
{
	KeyType Key;
	ValueType Value;
};

template <typename KeyType, typename ValueType>
class TMap
{
public:
	using ElementType = TPair<KeyType, ValueType>;

	class TIterator
	{
	public:
		explicit operator bool() const;
		TIterator& operator++();
		const KeyType& Key() const;
		ValueType& Value() const;
		void RemoveCurrent();
	};

	ValueType& Add(const KeyType& Key, const ValueType& Value);
	ValueType& Add(const KeyType& Key, ValueType&& Value);
	ValueType* Find(const KeyType& Key);
	const ValueType* Find(const KeyType& Key) const;
	ValueType FindRef(const KeyType& Key) const;
	bool Contains(const KeyType& Key) const;
	int32 Remove(const KeyType& Key);
	void Reset();
	void Empty();
	int32 Num() const;
	TIterator CreateIterator();
	ElementType* begin();
	ElementType* end();
	const ElementType* begin() const;
	const ElementType* end() const;
};

template <typename T>
struct TOptional
{
	TOptional();
	TOptional(const T& Value);
	bool IsSet() const;
	const T& GetValue() const;
};

// ============================================================================ Functions & delegates

template <typename FuncType>
class TFunction;

template <typename Ret, typename... ParamTypes>
class TFunction<Ret(ParamTypes...)>
{
public:
	TFunction();
	TFunction(std::nullptr_t);
	template <typename FunctorType, typename = typename std::enable_if<!std::is_same<typename std::decay<FunctorType>::type, TFunction>::value>::type>
	TFunction(FunctorType&& Functor)
	{
		static_assert(std::is_invocable_r<Ret, FunctorType, ParamTypes...>::value, "TFunction: callable does not match the signature");
	}
	TFunction(const TFunction& Other);
	TFunction(TFunction&& Other);
	TFunction& operator=(const TFunction& Other);
	TFunction& operator=(TFunction&& Other);
	TFunction& operator=(std::nullptr_t);
	explicit operator bool() const;
	Ret operator()(ParamTypes... Params) const;
};

class FDelegateHandle
{
public:
	bool IsValid() const;
	void Reset();
};

class FSimpleDelegate
{
public:
	template <typename FunctorType>
	static FSimpleDelegate CreateLambda(FunctorType&& Functor)
	{
		static_assert(std::is_invocable<FunctorType>::value, "FSimpleDelegate: lambda must take no arguments");
		return FSimpleDelegate();
	}
	bool IsBound() const;
	void Execute() const;
	bool ExecuteIfBound() const;
};

class FSimpleMulticastDelegate
{
public:
	template <typename UserClass>
	FDelegateHandle AddUObject(UserClass* Object, void (UserClass::*Method)());
	template <typename FunctorType>
	FDelegateHandle AddLambda(FunctorType&& Functor)
	{
		return FDelegateHandle();
	}
	bool Remove(FDelegateHandle Handle);
	void Broadcast() const;
};

//@section CoreDelegates: Core
using FApplicationLifetimeDelegate = FSimpleMulticastDelegate;

class FCoreDelegates
{
public:
	static FApplicationLifetimeDelegate ApplicationWillDeactivateDelegate;
	static FApplicationLifetimeDelegate ApplicationHasReactivatedDelegate;
	static FApplicationLifetimeDelegate ApplicationWillEnterBackgroundDelegate;
	static FApplicationLifetimeDelegate ApplicationHasEnteredForegroundDelegate;
};

//@section Async: Core
enum class EAsyncExecution
{
	TaskGraph,
	TaskGraphMainThread,
	Thread,
	ThreadIfForkSafe,
	ThreadPool,
	LargeThreadPool
};

template <typename ResultType>
class TFuture
{
public:
	bool IsReady() const;
};

template <typename CallableType>
TFuture<void> Async(EAsyncExecution Execution, CallableType&& Callable, TFunction<void()> CompletionCallback = nullptr)
{
	static_assert(std::is_invocable<CallableType>::value, "Async: callable must take no arguments");
	return TFuture<void>();
}

//@section Core
// ============================================================================ Smart pointers

enum class ESPMode : uint8
{
	NotThreadSafe = 0,
	ThreadSafe = 1
};

template <typename ObjectType, ESPMode Mode = ESPMode::ThreadSafe>
class TSharedRef;
template <typename ObjectType, ESPMode Mode = ESPMode::ThreadSafe>
class TSharedPtr;

template <typename ObjectType, ESPMode Mode>
class TSharedRef
{
public:
	template <typename OtherType, typename = typename std::enable_if<std::is_convertible<OtherType*, ObjectType*>::value>::type>
	TSharedRef(const TSharedRef<OtherType, Mode>& Other) : Object(Other.Object)
	{
	}
	TSharedRef(const TSharedRef& Other) = default;
	TSharedRef& operator=(const TSharedRef& Other) = default;
	ObjectType* operator->() const { return Object; }
	ObjectType& operator*() const { return *Object; }
	ObjectType& Get() const { return *Object; }
	TSharedPtr<ObjectType, Mode> ToSharedPtr() const { return TSharedPtr<ObjectType, Mode>(*this); }

private:
	TSharedRef() = default;
	ObjectType* Object = nullptr;

	template <typename, ESPMode>
	friend class TSharedRef;
	template <typename, ESPMode>
	friend class TSharedPtr;
	template <typename T, ESPMode M, typename... Args>
	friend TSharedRef<T, M> MakeShared(Args&&...);
	template <typename T>
	friend struct TSlateDecl;
	friend class SWidget;
	friend class SNullWidget;
};

template <typename ObjectType, ESPMode Mode>
class TSharedPtr
{
public:
	TSharedPtr() = default;
	TSharedPtr(std::nullptr_t) {}
	TSharedPtr(const TSharedPtr& Other) = default;
	template <typename OtherType, typename = typename std::enable_if<std::is_convertible<OtherType*, ObjectType*>::value>::type>
	TSharedPtr(const TSharedRef<OtherType, Mode>& Other) : Object(Other.Object)
	{
	}
	template <typename OtherType, typename = typename std::enable_if<std::is_convertible<OtherType*, ObjectType*>::value>::type>
	TSharedPtr(const TSharedPtr<OtherType, Mode>& Other) : Object(Other.Object)
	{
	}
	TSharedPtr& operator=(const TSharedPtr& Other) = default;
	TSharedPtr& operator=(std::nullptr_t)
	{
		Object = nullptr;
		return *this;
	}
	ObjectType* operator->() const { return Object; }
	ObjectType& operator*() const { return *Object; }
	ObjectType* Get() const { return Object; }
	bool IsValid() const { return Object != nullptr; }
	explicit operator bool() const { return Object != nullptr; }
	void Reset() { Object = nullptr; }
	TSharedRef<ObjectType, Mode> ToSharedRef() const
	{
		TSharedRef<ObjectType, Mode> Ref;
		Ref.Object = Object;
		return Ref;
	}

private:
	ObjectType* Object = nullptr;

	template <typename, ESPMode>
	friend class TSharedPtr;
};

template <typename ObjectType, ESPMode Mode = ESPMode::ThreadSafe, typename... ArgTypes>
TSharedRef<ObjectType, Mode> MakeShared(ArgTypes&&... Args)
{
	static_assert(std::is_constructible<ObjectType, ArgTypes...>::value, "MakeShared: no matching constructor");
	return TSharedRef<ObjectType, Mode>();
}

template <typename ObjectType>
class TUniquePtr
{
public:
	TUniquePtr();
	TUniquePtr(std::nullptr_t);
	TUniquePtr(TUniquePtr&& Other);
	template <typename OtherType>
	TUniquePtr(TUniquePtr<OtherType>&& Other);
	TUniquePtr& operator=(TUniquePtr&& Other);
	TUniquePtr(const TUniquePtr&) = delete;
	~TUniquePtr();
	ObjectType* operator->() const;
	ObjectType& operator*() const;
	ObjectType* Get() const;
	bool IsValid() const;
	explicit operator bool() const;
	void Reset(ObjectType* NewPtr = nullptr);
};

template <typename T, typename... ArgTypes>
TUniquePtr<T> MakeUnique(ArgTypes&&... Args)
{
	static_assert(std::is_constructible<T, ArgTypes...>::value, "MakeUnique: no matching constructor");
	return TUniquePtr<T>();
}

// ============================================================================ Strings

class FString
{
public:
	FString();
	FString(const TCHAR* Str);
	FString(const FString& Other);
	FString(FString&& Other);
	FString& operator=(const FString& Other);
	FString& operator=(FString&& Other);
	const TCHAR* operator*() const;
	bool IsEmpty() const;
	int32 Len() const;
	void Reset(int32 NewReservedSize = 0);
	FString& operator+=(const TCHAR* Str);
	FString& operator+=(const FString& Str);
	template <typename... Types>
	static FString Printf(TStubFormat<std::type_identity_t<Types>...> Fmt, Types... Args);
};

enum EName
{
	NAME_None
};

class FName
{
public:
	FName();
	FName(EName Name);
	FName(const TCHAR* Name);
	bool operator==(const FName& Other) const;
	bool operator!=(const FName& Other) const;
	FString ToString() const;
};

class FText
{
public:
	FText();
	static FText FromString(const FString& String);
	static FText FromString(FString&& String);
	static FText AsNumber(int32 Val);
	static FText AsNumber(float Val);
	static const FText& GetEmpty();
	bool IsEmpty() const;
	bool EqualTo(const FText& Other) const;
	const FString& ToString() const;
};

struct FChar
{
	static TCHAR ToUpper(TCHAR Char);
	static TCHAR ToLower(TCHAR Char);
};

// ============================================================================ Math

struct FMath
{
	template <typename T>
	static T Max(T A, T B);
	template <typename T>
	static T Min(T A, T B);
	template <typename T>
	static T Clamp(T X, T Lo, T Hi);
	template <typename T>
	static T Abs(T A);
	template <typename T, typename U>
	static T Lerp(const T& A, const T& B, const U& Alpha);
	static float Sin(float V);
	static double Sin(double V);
	static float Cos(float V);
	static double Cos(double V);
	static float Tan(float V);
	static float Atan(float V);
	static float Sqrt(float V);
	static int32 FloorToInt(float V);
	static int32 CeilToInt(float V);
	static int32 RoundToInt(float V);
	static float RoundToFloat(float V);
	template <typename T>
	static T DegreesToRadians(const T& Deg);
	template <typename T>
	static T RadiansToDegrees(const T& Rad);
	static float FixedTurn(float InCurrent, float InDesired, float InDeltaRate);
};

struct FMemory
{
	static void* Memcpy(void* Dest, const void* Src, SIZE_T Count);
	static void* Memzero(void* Dest, SIZE_T Count);
};

//@section PlatformTime: Core
struct FPlatformTime
{
	static double Seconds();
};

//@section Core
struct FRotator;

struct FVector
{
	double X;
	double Y;
	double Z;
	FVector();
	explicit FVector(double InF);
	FVector(double InX, double InY, double InZ);
	static const FVector ZeroVector;
	static const FVector OneVector;
	FVector operator+(const FVector& V) const;
	FVector operator-(const FVector& V) const;
	FVector operator*(double Scale) const;
	FVector operator*(const FVector& V) const;
	FVector operator/(double Scale) const;
	FVector& operator+=(const FVector& V);
	FVector& operator-=(const FVector& V);
	FVector& operator*=(double Scale);
	FVector& operator*=(const FVector& V);
	FVector operator-() const;
	bool Normalize(double Tolerance = 1e-8);
	FVector GetSafeNormal(double Tolerance = 1e-8) const;
	double Size() const;
	FRotator Rotation() const;
	static double Dist(const FVector& A, const FVector& B);
};

struct FVector2D
{
	double X;
	double Y;
	FVector2D();
	FVector2D(double InX, double InY);
	static const FVector2D ZeroVector;
	FVector2D operator+(const FVector2D& V) const;
	FVector2D operator-(const FVector2D& V) const;
	FVector2D operator*(double Scale) const;
	FVector2D operator/(double Scale) const;
	static double Distance(const FVector2D& A, const FVector2D& B);
};

struct FVector3f
{
	float X;
	float Y;
	float Z;
	FVector3f();
	FVector3f(float InX, float InY, float InZ);
	static FVector3f CrossProduct(const FVector3f& A, const FVector3f& B);
	FVector3f GetSafeNormal(float Tolerance = 1e-8f) const;
	bool Normalize(float Tolerance = 1e-8f);
};

struct FVector2f
{
	float X;
	float Y;
	FVector2f();
	FVector2f(float InX, float InY);
};

struct FVector4f
{
	float X;
	float Y;
	float Z;
	float W;
	FVector4f();
	FVector4f(float InX, float InY, float InZ, float InW);
};

struct FRotator
{
	double Pitch;
	double Yaw;
	double Roll;
	FRotator();
	FRotator(double InPitch, double InYaw, double InRoll);
	static const FRotator ZeroRotator;
	FVector Vector() const;
	FRotator operator+(const FRotator& R) const;
	FRotator operator*(double Scale) const;
	FRotator& operator+=(const FRotator& R);
	FRotator& operator*=(double Scale);
};

struct FTransform
{
	FTransform();
	FTransform(const FRotator& InRotation, const FVector& InTranslation, const FVector& InScale3D = FVector::OneVector);
	static const FTransform Identity;
};

struct FBox2D
{
	FVector2D Min;
	FVector2D Max;
	FBox2D();
	FBox2D(const FVector2D& InMin, const FVector2D& InMax);
};

struct FIntPoint
{
	int32 X;
	int32 Y;
	FIntPoint();
	FIntPoint(int32 InX, int32 InY);
};

struct FColor
{
	uint8 B;
	uint8 G;
	uint8 R;
	uint8 A;
	FColor();
	FColor(uint8 InR, uint8 InG, uint8 InB, uint8 InA = 255);
};

struct FLinearColor
{
	float R;
	float G;
	float B;
	float A;
	FLinearColor();
	FLinearColor(float InR, float InG, float InB, float InA = 1.0f);
	explicit FLinearColor(const FColor& Color);
	static const FLinearColor White;
	static const FLinearColor Black;
	static const FLinearColor Transparent;
	FColor ToFColor(const bool bSRGB) const;
	FLinearColor operator+(const FLinearColor& Other) const;
	FLinearColor operator-(const FLinearColor& Other) const;
	FLinearColor operator*(float Scalar) const;
};

//@section Margin: Core
struct FMargin
{
	float Left;
	float Top;
	float Right;
	float Bottom;
	FMargin();
	FMargin(float UniformMargin);
	FMargin(float Horizontal, float Vertical);
	FMargin(float InLeft, float InTop, float InRight, float InBottom);
};

//@section Threads: Core
// ============================================================================ Threads

class FCriticalSection
{
public:
	void Lock();
	void Unlock();
};

class FScopeLock
{
public:
	explicit FScopeLock(FCriticalSection* InSynchObject);
	~FScopeLock();
};

//@section CoreUObject
// ============================================================================ UObject

class UClass;
class UWorld;
class UObject;
class UPackage;

enum EObjectFlags
{
	RF_NoFlags = 0,
	RF_Public = 1,
	RF_Standalone = 2,
	RF_Transient = 4
};

class FObjectInitializer
{
public:
	FObjectInitializer();
};

class UObject
{
public:
	UObject();
	UObject(const FObjectInitializer& ObjectInitializer);
	virtual ~UObject();
	virtual UWorld* GetWorld() const;
	static UClass* StaticClass();
	FName GetFName() const;
};

class UClass : public UObject
{
};

UPackage* GetTransientPackage();

template <typename T>
T* NewObject(UObject* Outer = (UObject*)GetTransientPackage(), FName Name = NAME_None, EObjectFlags Flags = RF_NoFlags);

template <typename T>
T* LoadObject(UObject* Outer, const TCHAR* Name, const TCHAR* Filename = nullptr, uint32 LoadFlags = 0, void* Sandbox = nullptr);

template <typename To, typename From>
To* Cast(From* Src);
template <typename T>
class TObjectPtr;
template <typename To, typename From>
To* Cast(const TObjectPtr<From>& Src);

FName MakeUniqueObjectName(UObject* Outer, const UClass* Class, FName BaseName = NAME_None);

template <typename T>
const T* GetDefault();

template <typename T>
class TSubclassOf
{
public:
	TSubclassOf();
	TSubclassOf(UClass* From);
	TSubclassOf& operator=(UClass* From);
};

template <typename T>
class TObjectPtr
{
public:
	TObjectPtr();
	TObjectPtr(std::nullptr_t);
	TObjectPtr(T* Object);
	template <typename U, typename = typename std::enable_if<std::is_convertible<U*, T*>::value>::type>
	TObjectPtr(U* Object);
	TObjectPtr& operator=(T* Object);
	TObjectPtr& operator=(std::nullptr_t);
	operator T*() const;
	T* operator->() const;
	T& operator*() const;
	T* Get() const;
};

template <typename T>
class TWeakObjectPtr
{
public:
	TWeakObjectPtr();
	TWeakObjectPtr(std::nullptr_t);
	TWeakObjectPtr(const T* Object);
	template <typename U, typename = typename std::enable_if<std::is_convertible<U*, T*>::value>::type>
	TWeakObjectPtr(const TObjectPtr<U>& Object);
	TWeakObjectPtr& operator=(const T* Object);
	T* Get() const;
	bool IsValid() const;
	void Reset();
	T* operator->() const;
};

class FSoftObjectPath
{
public:
	FSoftObjectPath();
	FSoftObjectPath(const TCHAR* Path);
	FSoftObjectPath(const FString& Path);
};

template <typename T>
class TSoftObjectPtr
{
public:
	TSoftObjectPtr();
	explicit TSoftObjectPtr(const FSoftObjectPath& Path);
	T* LoadSynchronous() const;
	T* Get() const;
	bool IsNull() const;
};

template <typename EnumType>
class TEnumAsByte
{
public:
	TEnumAsByte();
	TEnumAsByte(EnumType InValue);
	TEnumAsByte& operator=(EnumType InValue);
	operator EnumType() const;
};

//@section Modules: Core
// ============================================================================ Modules

class IModuleInterface
{
public:
	virtual ~IModuleInterface();
};
class FDefaultGameModuleImpl : public IModuleInterface
{
};
#define IMPLEMENT_PRIMARY_GAME_MODULE(ModuleImplClass, ModuleName, GameName) \
	[[maybe_unused]] static ModuleImplClass* StubModule_##ModuleName = nullptr;

//@section SlateCore: Core CoreUObject Margin Attribute InputCore
// ============================================================================ Slate core types

class SWidget;
class UTexture2D;

struct EVisibility
{
	static const EVisibility Visible;
	static const EVisibility Collapsed;
	static const EVisibility Hidden;
	static const EVisibility HitTestInvisible;
	static const EVisibility SelfHitTestInvisible;
	static const EVisibility All;
	bool operator==(const EVisibility& Other) const;
	bool operator!=(const EVisibility& Other) const;
	bool IsVisible() const;
};

enum EHorizontalAlignment
{
	HAlign_Fill,
	HAlign_Left,
	HAlign_Center,
	HAlign_Right
};
enum EVerticalAlignment
{
	VAlign_Fill,
	VAlign_Top,
	VAlign_Center,
	VAlign_Bottom
};

//@section TextJustify: Core
namespace ETextJustify
{
enum Type
{
	Left,
	Center,
	Right
};
}

//@section SlateCore
class FSlateColor
{
public:
	FSlateColor();
	FSlateColor(const FLinearColor& InColor);
	const FLinearColor& GetSpecifiedColor() const;
};

namespace ESlateBrushDrawType
{
enum Type
{
	NoDrawType,
	Box,
	Border,
	Image,
	RoundedBox
};
}

struct FSlateBrush
{
	FSlateBrush();
	virtual ~FSlateBrush();
	FVector2D ImageSize;
	FMargin Margin;
	FSlateColor TintColor;
	TEnumAsByte<ESlateBrushDrawType::Type> DrawAs;
	void SetResourceObject(UObject* InResourceObject);
	UObject* GetResourceObject() const;
};

//@section SlateColorBrush: SlateCore
struct FSlateColorBrush : public FSlateBrush
{
	FSlateColorBrush(const FLinearColor& InColor);
};

//@section SlateTypes: SlateCore SlateFontInfo
struct FSlateSound
{
};

struct FButtonStyle
{
	FButtonStyle();
	FButtonStyle& SetNormal(const FSlateBrush& InNormal);
	FButtonStyle& SetHovered(const FSlateBrush& InHovered);
	FButtonStyle& SetPressed(const FSlateBrush& InPressed);
	FButtonStyle& SetDisabled(const FSlateBrush& InDisabled);
	FButtonStyle& SetNormalPadding(const FMargin& InNormalPadding);
	FButtonStyle& SetPressedPadding(const FMargin& InPressedPadding);
};

//@section SlateFontInfo: Core CoreUObject
struct FFontOutlineSettings
{
	int32 OutlineSize;
	FLinearColor OutlineColor;
	FFontOutlineSettings();
};

struct FSlateFontInfo
{
	FSlateFontInfo();
	FFontOutlineSettings OutlineSettings;
	int32 LetterSpacing;
	float Size;
};

//@section CoreStyle: SlateCore SlateFontInfo
class FCoreStyle
{
public:
	static FSlateFontInfo GetDefaultFontStyle(const FName InTypefaceFontName, const float InSize, const FFontOutlineSettings& InOutlineSettings = FFontOutlineSettings());
};

//@section SlateCore
struct FOptionalSize
{
	FOptionalSize();
	FOptionalSize(const float SpecifiedSize);
};

class FReply
{
public:
	static FReply Handled();
	static FReply Unhandled();
	FReply& CaptureMouse(TSharedRef<SWidget> InMouseCaptor);
	FReply& ReleaseMouseCapture();
	bool IsEventHandled() const;
};

struct FSlateLayoutTransform
{
	float GetScale() const;
};

class FGeometry
{
public:
	FVector2D AbsoluteToLocal(FVector2D AbsoluteCoordinate) const;
	FVector2D LocalToAbsolute(FVector2D LocalCoordinate) const;
	FVector2D GetLocalSize() const;
	FVector2D GetAbsoluteSize() const;
	const FSlateLayoutTransform& GetAccumulatedLayoutTransform() const;
};

//@section InputCore: Core
struct FKey
{
	FKey();
	bool operator==(const FKey& Other) const;
	bool operator!=(const FKey& Other) const;
};

struct EKeys
{
	static const FKey LeftMouseButton;
	static const FKey RightMouseButton;
	static const FKey MiddleMouseButton;
	static const FKey Escape;
	static const FKey SpaceBar;
	static const FKey Left;
	static const FKey Right;
	static const FKey Up;
	static const FKey Down;
	static const FKey A;
	static const FKey D;
	static const FKey S;
	static const FKey W;
	static const FKey Android_Back;
};

//@section SlateCore
class FInputEvent
{
public:
	virtual ~FInputEvent();
};

class FPointerEvent : public FInputEvent
{
public:
	const FVector2D& GetScreenSpacePosition() const;
	FKey GetEffectingButton() const;
	float GetWheelDelta() const;
	uint32 GetPointerIndex() const;
	uint32 GetUserIndex() const;
	bool IsTouchEvent() const;
};

class FKeyEvent : public FInputEvent
{
public:
	FKey GetKey() const;
	uint32 GetCharacter() const;
};

struct FCaptureLostEvent
{
	uint32 UserIndex;
	uint32 PointerIndex;
};

struct FPaintArgs;
class FSlateWindowElementList;
class FSlateRect;
class FWidgetStyle;

enum class EFocusCause : uint8
{
	Mouse,
	Navigation,
	SetDirectly,
	Cleared,
	OtherWidgetLostFocus,
	WindowActivate
};

//@section Attribute: Core
// ============================================================================ Slate widgets

// Attribute: a value or a function evaluated when read.
template <typename ObjectType>
class TAttribute
{
public:
	TAttribute();
	template <typename OtherType, typename = typename std::enable_if<std::is_constructible<ObjectType, const OtherType&>::value>::type>
	TAttribute(const OtherType& InInitialValue);
	TAttribute(const TAttribute& Other);
	TAttribute& operator=(const TAttribute& Other);
	template <typename LambdaType>
	static TAttribute CreateLambda(LambdaType&& InCallable)
	{
		static_assert(std::is_convertible<decltype(InCallable()), ObjectType>::value, "TAttribute: lambda returns the wrong type");
		return TAttribute();
	}
	const ObjectType& Get() const;
	bool IsSet() const;
};

//@section SlateCore
class FOnClicked
{
public:
	template <typename FunctorType>
	static FOnClicked CreateLambda(FunctorType&& Functor)
	{
		static_assert(std::is_same<decltype(Functor()), FReply>::value, "FOnClicked: lambda must return FReply");
		return FOnClicked();
	}
	FReply Execute() const;
	bool IsBound() const;
};

class SWidget : public std::enable_shared_from_this<SWidget>
{
public:
	virtual ~SWidget();
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	virtual FReply OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent);
	virtual FReply OnTouchMoved(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent);
	virtual FReply OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent);
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);
	virtual FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);
	virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent);
	virtual bool SupportsKeyboardFocus() const;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime);
	void SetVisibility(TAttribute<EVisibility> InVisibility);
	void SetEnabled(TAttribute<bool> InEnabledState);

protected:
	template <typename OtherType>
	static TSharedRef<OtherType> SharedThis(OtherType* ThisPtr)
	{
		TSharedRef<OtherType> Ref;
		Ref.Object = ThisPtr;
		return Ref;
	}
};

//@section SNullWidget: SlateCore
class SNullWidget
{
public:
	static TSharedRef<SWidget> NullWidget;
};

//@section SlateCore
// Base arguments every widget accepts.
template <typename WidgetType>
struct TSlateBaseNamedArgs
{
	using WidgetArgsType = typename WidgetType::FArguments;
	WidgetArgsType& Me() { return *static_cast<WidgetArgsType*>(this); }

	TAttribute<EVisibility> _Visibility;
	WidgetArgsType& Visibility(const TAttribute<EVisibility>& InAttribute) { _Visibility = InAttribute; return Me(); }
	template <typename LambdaType>
	WidgetArgsType& Visibility_Lambda(LambdaType&& InFunctor) { _Visibility = TAttribute<EVisibility>::CreateLambda(std::forward<LambdaType>(InFunctor)); return Me(); }

	TAttribute<bool> _IsEnabled;
	WidgetArgsType& IsEnabled(const TAttribute<bool>& InAttribute) { _IsEnabled = InAttribute; return Me(); }
	template <typename LambdaType>
	WidgetArgsType& IsEnabled_Lambda(LambdaType&& InFunctor) { _IsEnabled = TAttribute<bool>::CreateLambda(std::forward<LambdaType>(InFunctor)); return Me(); }

	TAttribute<FText> _ToolTipText;
	WidgetArgsType& ToolTipText(const TAttribute<FText>& InAttribute) { _ToolTipText = InAttribute; return Me(); }
};

#define SLATE_BEGIN_ARGS(WidgetType) \
public: \
	struct FArguments : public TSlateBaseNamedArgs<WidgetType> \
	{ \
		typedef FArguments WidgetArgsType; \
		FArguments()

#define SLATE_END_ARGS() \
	};

#define SLATE_ARGUMENT(ArgType, ArgName) \
	ArgType _##ArgName; \
	WidgetArgsType& ArgName(ArgType InArg) \
	{ \
		_##ArgName = InArg; \
		return *this; \
	}

#define SLATE_STYLE_ARGUMENT(ArgType, ArgName) \
	const ArgType* _##ArgName = nullptr; \
	WidgetArgsType& ArgName(const ArgType* InArg) \
	{ \
		_##ArgName = InArg; \
		return *this; \
	}

#define SLATE_ATTRIBUTE(AttrType, AttrName) \
	TAttribute<AttrType> _##AttrName; \
	WidgetArgsType& AttrName(const TAttribute<AttrType>& InAttribute) \
	{ \
		_##AttrName = InAttribute; \
		return *this; \
	} \
	template <typename LambdaType> \
	WidgetArgsType& AttrName##_Lambda(LambdaType&& InFunctor) \
	{ \
		_##AttrName = TAttribute<AttrType>::CreateLambda(std::forward<LambdaType>(InFunctor)); \
		return *this; \
	}

#define SLATE_EVENT(DelegateName, EventName) \
	DelegateName _##EventName; \
	WidgetArgsType& EventName(const DelegateName& InDelegate) \
	{ \
		_##EventName = InDelegate; \
		return *this; \
	} \
	template <typename LambdaType> \
	WidgetArgsType& EventName##_Lambda(LambdaType&& InFunctor) \
	{ \
		_##EventName = DelegateName::CreateLambda(std::forward<LambdaType>(InFunctor)); \
		return *this; \
	}

struct FStubNamedSlot
{
	TSharedPtr<SWidget> Widget;
};

#define SLATE_DEFAULT_SLOT(DeclarationType, SlotName) \
	FStubNamedSlot _##SlotName; \
	DeclarationType& operator[](const TSharedRef<SWidget>& InChild) \
	{ \
		_##SlotName.Widget = InChild; \
		return *this; \
	}

// Slot arguments shared by panels.
template <typename Derived>
struct TStubSlotArguments
{
	Derived& Me() { return *static_cast<Derived*>(this); }
	Derived& HAlign(EHorizontalAlignment InHAlign) { return Me(); }
	Derived& VAlign(EVerticalAlignment InVAlign) { return Me(); }
	Derived& Padding(const TAttribute<FMargin>& InPadding) { return Me(); }
	Derived& Padding(float Uniform) { return Me(); }
	Derived& Padding(float Horizontal, float Vertical) { return Me(); }
	Derived& Padding(float Left, float Top, float Right, float Bottom) { return Me(); }
	Derived& operator[](const TSharedRef<SWidget>& InChildWidget) { return Me(); }
};

// Makes widgets the way SNew / SAssignNew do.
template <typename WidgetType>
struct TSlateDecl
{
	template <typename ExposeAsWidgetType>
	TSlateDecl& Expose(TSharedPtr<ExposeAsWidgetType>& OutVarToInit)
	{
		static_assert(std::is_convertible<WidgetType*, ExposeAsWidgetType*>::value, "SAssignNew: pointer type does not match the widget");
		return *this;
	}
	TSharedRef<WidgetType> operator<<=(const typename WidgetType::FArguments& InArgs) const
	{
		// Slate default-constructs the widget, then calls Construct with the arguments.
		static_assert(std::is_default_constructible<WidgetType>::value, "Slate widgets need a default constructor");
		TSharedRef<WidgetType> Ref;
		Ref.Object = static_cast<WidgetType*>(StubAnyPtr());
		Ref.Object->Construct(InArgs);
		return Ref;
	}
};

#define SNew(WidgetType, ...) TSlateDecl<WidgetType>() <<= WidgetType::FArguments()
#define SAssignNew(ExposeAs, WidgetType, ...) TSlateDecl<WidgetType>().Expose(ExposeAs) <<= WidgetType::FArguments()

class FStubChildSlot
{
public:
	FStubChildSlot& operator[](const TSharedRef<SWidget>& InChild);
	FStubChildSlot& HAlign(EHorizontalAlignment InHAlign);
	FStubChildSlot& VAlign(EVerticalAlignment InVAlign);
	FStubChildSlot& Padding(const TAttribute<FMargin>& InPadding);
};

class SCompoundWidget : public SWidget
{
protected:
	FStubChildSlot ChildSlot;
};

class SLeafWidget : public SWidget
{
};

class SPanel : public SWidget
{
};

//@section SOverlay: SlateCore
class SOverlay : public SPanel
{
public:
	struct FOverlaySlot
	{
		struct FSlotArguments : public TStubSlotArguments<FSlotArguments>
		{
		};
	};
	static FOverlaySlot::FSlotArguments Slot();

	SLATE_BEGIN_ARGS(SOverlay) {}
		FArguments& operator+(FOverlaySlot::FSlotArguments& SlotToAdd);
		FArguments& operator+(FOverlaySlot::FSlotArguments&& SlotToAdd);
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	FOverlaySlot::FSlotArguments AddSlot(int32 ZOrder = -1);
};

//@section SBoxPanel: SlateCore
class SBoxPanel : public SPanel
{
};

class SHorizontalBox : public SBoxPanel
{
public:
	struct FSlot
	{
		struct FSlotArguments : public TStubSlotArguments<FSlotArguments>
		{
			FSlotArguments& AutoWidth();
			FSlotArguments& FillWidth(const TAttribute<float>& StretchCoefficient);
			FSlotArguments& MaxWidth(const TAttribute<float>& InMaxWidth);
		};
	};
	using FScopedWidgetSlotArguments = FSlot::FSlotArguments;
	static FSlot::FSlotArguments Slot();

	SLATE_BEGIN_ARGS(SHorizontalBox) {}
		FArguments& operator+(FSlot::FSlotArguments& SlotToAdd);
		FArguments& operator+(FSlot::FSlotArguments&& SlotToAdd);
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	FScopedWidgetSlotArguments AddSlot();
};

class SVerticalBox : public SBoxPanel
{
public:
	struct FSlot
	{
		struct FSlotArguments : public TStubSlotArguments<FSlotArguments>
		{
			FSlotArguments& AutoHeight();
			FSlotArguments& FillHeight(const TAttribute<float>& StretchCoefficient);
			FSlotArguments& MaxHeight(const TAttribute<float>& InMaxHeight);
		};
	};
	using FScopedWidgetSlotArguments = FSlot::FSlotArguments;
	static FSlot::FSlotArguments Slot();

	SLATE_BEGIN_ARGS(SVerticalBox) {}
		FArguments& operator+(FSlot::FSlotArguments& SlotToAdd);
		FArguments& operator+(FSlot::FSlotArguments&& SlotToAdd);
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	FScopedWidgetSlotArguments AddSlot();
};

//@section SBox: SlateCore
class SBox : public SPanel
{
public:
	SLATE_BEGIN_ARGS(SBox) {}
		SLATE_ARGUMENT(EHorizontalAlignment, HAlign)
		SLATE_ARGUMENT(EVerticalAlignment, VAlign)
		SLATE_ATTRIBUTE(FMargin, Padding)
		SLATE_ATTRIBUTE(FOptionalSize, WidthOverride)
		SLATE_ATTRIBUTE(FOptionalSize, HeightOverride)
		SLATE_ATTRIBUTE(FOptionalSize, MinDesiredWidth)
		SLATE_ATTRIBUTE(FOptionalSize, MinDesiredHeight)
		SLATE_ATTRIBUTE(FOptionalSize, MaxDesiredWidth)
		SLATE_ATTRIBUTE(FOptionalSize, MaxDesiredHeight)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void SetContent(const TSharedRef<SWidget>& InContent);
};

//@section SBorder: SlateCore
class SBorder : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBorder) {}
		SLATE_ARGUMENT(EHorizontalAlignment, HAlign)
		SLATE_ARGUMENT(EVerticalAlignment, VAlign)
		SLATE_ATTRIBUTE(FMargin, Padding)
		SLATE_ATTRIBUTE(const FSlateBrush*, BorderImage)
		SLATE_ATTRIBUTE(FSlateColor, BorderBackgroundColor)
		SLATE_ATTRIBUTE(FLinearColor, ColorAndOpacity)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void SetContent(const TSharedRef<SWidget>& InContent);
};

//@section SImage: SlateCore
class SImage : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SImage) {}
		SLATE_ATTRIBUTE(const FSlateBrush*, Image)
		SLATE_ATTRIBUTE(FSlateColor, ColorAndOpacity)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};

//@section STextBlock: SlateCore TextJustify SlateTypes
class STextBlock : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(STextBlock) {}
		SLATE_ATTRIBUTE(FText, Text)
		SLATE_ATTRIBUTE(FSlateFontInfo, Font)
		SLATE_ATTRIBUTE(FSlateColor, ColorAndOpacity)
		SLATE_ATTRIBUTE(FVector2D, ShadowOffset)
		SLATE_ATTRIBUTE(FLinearColor, ShadowColorAndOpacity)
		SLATE_ATTRIBUTE(ETextJustify::Type, Justification)
		SLATE_ATTRIBUTE(float, WrapTextAt)
		SLATE_ATTRIBUTE(bool, AutoWrapText)
		SLATE_ATTRIBUTE(FMargin, Margin)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void SetText(const TAttribute<FText>& InText);
};

//@section SButton: SBorder SlateTypes
class SButton : public SBorder
{
public:
	SLATE_BEGIN_ARGS(SButton) {}
		SLATE_STYLE_ARGUMENT(FButtonStyle, ButtonStyle)
		SLATE_ARGUMENT(EHorizontalAlignment, HAlign)
		SLATE_ARGUMENT(EVerticalAlignment, VAlign)
		SLATE_ATTRIBUTE(FMargin, ContentPadding)
		SLATE_ARGUMENT(bool, IsFocusable)
		SLATE_EVENT(FOnClicked, OnClicked)
		SLATE_EVENT(FSimpleDelegate, OnPressed)
		SLATE_EVENT(FSimpleDelegate, OnReleased)
		SLATE_EVENT(FSimpleDelegate, OnHovered)
		SLATE_EVENT(FSimpleDelegate, OnUnhovered)
		SLATE_ATTRIBUTE(FText, Text)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};

//@section SSpacer: SlateCore
class SSpacer : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SSpacer) {}
		SLATE_ATTRIBUTE(FVector2D, Size)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};

//@section SSafeZone: SBox
class SSafeZone : public SBox
{
public:
	SLATE_BEGIN_ARGS(SSafeZone) {}
		SLATE_ARGUMENT(EHorizontalAlignment, HAlign)
		SLATE_ARGUMENT(EVerticalAlignment, VAlign)
		SLATE_ATTRIBUTE(FMargin, Padding)
		SLATE_ARGUMENT(bool, IsTitleSafe)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};

//@section SlateApplication: SlateCore
class FSlateApplication
{
public:
	static bool IsInitialized();
	static FSlateApplication& Get();
	bool SetKeyboardFocus(const TSharedPtr<SWidget>& OptionalWidgetToFocus, EFocusCause ReasonFocusIsChanging = EFocusCause::SetDirectly);
};

//@section Texture: CoreUObject EngineFwd
// ============================================================================ Engine: rendering assets

enum EPixelFormat : uint8
{
	PF_Unknown,
	PF_B8G8R8A8,
	PF_R8G8B8A8
};

enum TextureFilter : int
{
	TF_Nearest,
	TF_Bilinear,
	TF_Trilinear,
	TF_Default
};

enum TextureAddress : int
{
	TA_Wrap,
	TA_Clamp,
	TA_Mirror
};

//@section TextureResource: Texture
enum EBulkDataLockFlags
{
	LOCK_READ_ONLY = 1,
	LOCK_READ_WRITE = 2
};

class FByteBulkData
{
public:
	void* Lock(uint32 LockFlags);
	void Unlock() const;
	void* Realloc(int64 NumBytes);
};

struct FTexture2DMipMap
{
	FByteBulkData BulkData;
	int32 SizeX;
	int32 SizeY;
};

//@section Texture
struct FTexture2DMipMap;

struct FTexturePlatformData
{
	int32 SizeX;
	int32 SizeY;
	TIndirectArray<FTexture2DMipMap> Mips;
};

struct FUpdateTextureRegion2D
{
	FUpdateTextureRegion2D(uint32 InDestX, uint32 InDestY, int32 InSrcX, int32 InSrcY, uint32 InWidth, uint32 InHeight);
};

class UTexture : public UObject
{
public:
	uint8 SRGB : 1;
	uint8 NeverStream : 1;
	TEnumAsByte<TextureFilter> Filter;
	void UpdateResource();
};

class UTexture2D : public UTexture
{
public:
	TEnumAsByte<TextureAddress> AddressX;
	TEnumAsByte<TextureAddress> AddressY;
	static UTexture2D* CreateTransient(int32 InSizeX, int32 InSizeY, EPixelFormat InFormat = PF_B8G8R8A8, const FName InName = NAME_None);
	FTexturePlatformData* GetPlatformData();
	const FTexturePlatformData* GetPlatformData() const;
	int32 GetSizeX() const;
	int32 GetSizeY() const;
	void UpdateTextureRegions(int32 MipIndex, uint32 NumRegions, const FUpdateTextureRegion2D* Regions, uint32 SrcPitch, uint32 SrcBpp, uint8* SrcData,
		TFunction<void(uint8* SrcData, const FUpdateTextureRegion2D* Regions)> DataCleanupFunc = nullptr);
};

//@section MaterialDomain: Core
enum EMaterialDomain : int
{
	MD_Surface,
	MD_DeferredDecal,
	MD_LightFunction,
	MD_Volume,
	MD_PostProcess,
	MD_UI
};

//@section MaterialInterface: CoreUObject EngineFwd
class UMaterialInterface : public UObject
{
};

//@section Material: MaterialInterface MaterialDomain
class UMaterial : public UMaterialInterface
{
public:
	static UMaterial* GetDefaultMaterial(EMaterialDomain Domain);
};

//@section MaterialInstanceDynamic: MaterialInterface
class UMaterialInstanceDynamic : public UMaterialInterface
{
public:
	static UMaterialInstanceDynamic* Create(UMaterialInterface* ParentMaterial, UObject* InOuter);
	void SetTextureParameterValue(FName ParameterName, class UTexture* Value);
	void SetVectorParameterValue(FName ParameterName, const FLinearColor& Value);
	void SetScalarParameterValue(FName ParameterName, float Value);
};

//@section MeshDescription: Core CoreUObject
// Mesh description (MeshDescription / StaticMeshDescription modules).
struct FVertexID
{
	FVertexID();
	explicit FVertexID(int32 InValue);
};
struct FVertexInstanceID
{
	FVertexInstanceID();
	explicit FVertexInstanceID(int32 InValue);
};
struct FPolygonGroupID
{
	FPolygonGroupID();
};
struct FTriangleID
{
};
struct FEdgeID
{
};

template <typename AttributeType>
class TVertexAttributesRef
{
public:
	AttributeType& operator[](FVertexID Element) const;
};

template <typename AttributeType>
class TVertexInstanceAttributesRef
{
public:
	AttributeType& operator[](FVertexInstanceID Element) const;
	void Set(FVertexInstanceID Element, int32 AttributeIndex, const AttributeType& Value) const;
	void SetNumChannels(int32 NumChannels) const;
	int32 GetNumChannels() const;
};

template <typename AttributeType>
class TPolygonGroupAttributesRef
{
public:
	AttributeType& operator[](FPolygonGroupID Element) const;
};

class FMeshDescription
{
public:
	FVertexID CreateVertex();
	FVertexInstanceID CreateVertexInstance(const FVertexID VertexID);
	FPolygonGroupID CreatePolygonGroup();
	FTriangleID CreateTriangle(FPolygonGroupID PolygonGroupID, TArrayView<const FVertexInstanceID> VertexInstanceIDs, TArray<FEdgeID>* OutEdgeIDs = nullptr);
	TVertexAttributesRef<FVector3f> GetVertexPositions();
};

//@section StaticMeshAttributes: MeshDescription
class FStaticMeshAttributes
{
public:
	explicit FStaticMeshAttributes(FMeshDescription& InMeshDescription);
	void Register(bool bKeepExistingAttribute = false);
	TVertexAttributesRef<FVector3f> GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector3f> GetVertexInstanceTangents();
	TVertexInstanceAttributesRef<float> GetVertexInstanceBinormalSigns();
	TVertexInstanceAttributesRef<FVector4f> GetVertexInstanceColors();
	TVertexInstanceAttributesRef<FVector2f> GetVertexInstanceUVs();
	TPolygonGroupAttributesRef<FName> GetPolygonGroupMaterialSlotNames();
};

//@section StaticMesh: CoreUObject EngineFwd
struct FMeshUVChannelInfo
{
	bool bInitialized;
	bool bOverrideDensities;
};

struct FStaticMaterial
{
	FStaticMaterial();
	FStaticMaterial(UMaterialInterface* InMaterialInterface, FName InMaterialSlotName = NAME_None);
	UMaterialInterface* MaterialInterface;
	FName MaterialSlotName;
	FMeshUVChannelInfo UVChannelData;
};

class UStaticMesh : public UObject
{
public:
	static UClass* StaticClass();
	struct FBuildMeshDescriptionsParams
	{
		FBuildMeshDescriptionsParams();
		bool bMarkPackageDirty;
		bool bUseHashAsGuid;
		bool bBuildSimpleCollision;
		bool bCommitMeshDescription;
		bool bFastBuild;
		bool bAllowCpuAccess;
	};
	TArray<FStaticMaterial>& GetStaticMaterials();
	const TArray<FStaticMaterial>& GetStaticMaterials() const;
	bool BuildFromMeshDescriptions(const TArray<const FMeshDescription*>& MeshDescriptions, const FBuildMeshDescriptionsParams& Params = FBuildMeshDescriptionsParams());
};

//@section SoundBase
// ============================================================================ Engine: audio

enum ESoundGroup : int
{
	SOUNDGROUP_Default,
	SOUNDGROUP_Effects,
	SOUNDGROUP_UI,
	SOUNDGROUP_Music,
	SOUNDGROUP_Voice
};

enum class EVirtualizationMode : uint8
{
	Disabled,
	PlayWhenSilent,
	Restart
};

class USoundBase : public UObject
{
public:
	USoundBase();
	USoundBase(const FObjectInitializer& ObjectInitializer);
	float Duration;
	EVirtualizationMode VirtualizationMode;
};

//@section SoundWave: SoundBase
class USoundWave : public USoundBase
{
public:
	USoundWave();
	USoundWave(const FObjectInitializer& ObjectInitializer);
	int32 NumChannels;
	uint8 bLooping : 1;
	TEnumAsByte<ESoundGroup> SoundGroup;
	void SetSampleRate(uint32 InSampleRate);
};

//@section SoundWaveProcedural: SoundWave
class USoundWaveProcedural : public USoundWave
{
public:
	using Super = USoundWave;
	USoundWaveProcedural(const FObjectInitializer& ObjectInitializer);
	virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples);
};

//@section EngineFwd: CoreUObject
// ============================================================================ Engine: gameplay framework

class UActorComponent;
class USceneComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;
class UCameraComponent;
class UAudioComponent;
class AActor;
class AController;
class APawn;
class APlayerController;
class AGameModeBase;
class AHUD;
class UGameInstance;
class UGameViewportClient;
class UCanvas;
class UFont;
class UEngine;
class UTouchInterface;
class USaveGame;
class UTexture;
class UTexture2D;
class UMaterialInterface;
class UMaterial;
class UMaterialInstanceDynamic;
class UStaticMesh;
class USoundBase;
class USoundWave;
class FMeshDescription;
struct FCanvasItem;
class SWidget;

//@section EngineTypes: Core
namespace EEndPlayReason
{
enum Type : int
{
	Destroyed,
	LevelTransition,
	EndPlayInEditor,
	RemovedFromWorld,
	Quit
};
}

namespace ECollisionEnabled
{
enum Type : int
{
	NoCollision,
	QueryOnly,
	PhysicsOnly,
	QueryAndPhysics
};
}

namespace EComponentMobility
{
enum Type : int
{
	Static,
	Stationary,
	Movable
};
}

//@section Actor: SceneComponent
struct FActorTickFunction
{
	uint8 bCanEverTick : 1;
	uint8 bTickEvenWhenPaused : 1;
	uint8 bStartWithTickEnabled : 1;
};

//@section ActorComponent: CoreUObject EngineFwd EngineTypes
class UActorComponent : public UObject
{
public:
	uint8 bAutoActivate : 1;
	void RegisterComponent();
	void DestroyComponent(bool bPromoteChildren = false);
	void SetCanEverAffectNavigation(bool bRelevant);
	AActor* GetOwner() const;
};

//@section SceneComponent: ActorComponent
class USceneComponent : public UActorComponent
{
public:
	void SetupAttachment(USceneComponent* InParent, FName InSocketName = NAME_None);
	void SetWorldLocationAndRotation(FVector NewLocation, FRotator NewRotation, bool bSweep = false, void* OutSweepHitResult = nullptr, int Teleport = 0);
	void SetWorldTransform(const FTransform& NewTransform, bool bSweep = false, void* OutSweepHitResult = nullptr, int Teleport = 0);
	void SetRelativeTransform(const FTransform& NewTransform, bool bSweep = false, void* OutSweepHitResult = nullptr, int Teleport = 0);
	void SetRelativeLocation(FVector NewLocation, bool bSweep = false, void* OutSweepHitResult = nullptr, int Teleport = 0);
	void SetVisibility(bool bNewVisibility, bool bPropagateToChildren = false);
	bool IsVisible() const;
	void SetMobility(EComponentMobility::Type NewMobility);
	void SetUsingAbsoluteLocation(bool bInAbsoluteLocation);
	void SetUsingAbsoluteRotation(bool bInAbsoluteRotation);
	FVector GetComponentLocation() const;
};

//@section PrimitiveComponent: SceneComponent
class UPrimitiveComponent : public USceneComponent
{
public:
	uint8 bReceivesDecals : 1;
	void SetCollisionEnabled(ECollisionEnabled::Type NewType);
	void SetGenerateOverlapEvents(bool bInGenerateOverlapEvents);
	void SetCastShadow(bool NewCastShadow);
	void SetTranslucentSortPriority(int32 NewTranslucentSortPriority);
	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const;
	virtual void SetMaterial(int32 ElementIndex, UMaterialInterface* Material);
};

//@section StaticMeshComponent: PrimitiveComponent
class UMeshComponent : public UPrimitiveComponent
{
};

class UStaticMeshComponent : public UMeshComponent
{
public:
	virtual bool SetStaticMesh(UStaticMesh* NewMesh);
	UStaticMesh* GetStaticMesh() const;
};

//@section CameraComponent: SceneComponent
namespace EAutoExposureMethod_Stub
{
}
enum EAutoExposureMethod : int
{
	AEM_Histogram,
	AEM_Basic,
	AEM_Manual
};

struct FPostProcessSettings
{
	uint8 bOverride_AutoExposureMethod : 1;
	uint8 bOverride_AutoExposureBias : 1;
	uint8 bOverride_AutoExposureApplyPhysicalCameraExposure : 1;
	uint8 bOverride_BloomIntensity : 1;
	uint8 bOverride_VignetteIntensity : 1;
	uint8 bOverride_MotionBlurAmount : 1;
	TEnumAsByte<EAutoExposureMethod> AutoExposureMethod;
	float AutoExposureBias;
	uint32 AutoExposureApplyPhysicalCameraExposure : 1;
	float BloomIntensity;
	float VignetteIntensity;
	float MotionBlurAmount;
};

class UCameraComponent : public USceneComponent
{
public:
	uint8 bConstrainAspectRatio : 1;
	FPostProcessSettings PostProcessSettings;
	float PostProcessBlendWeight;
	void SetFieldOfView(float InFieldOfView);
};

//@section AudioComponent: SceneComponent
class UAudioComponent : public USceneComponent
{
public:
	uint8 bIsUISound : 1;
	uint8 bAllowSpatialization : 1;
	void SetSound(USoundBase* NewSound);
	void Play(float StartTime = 0.f);
	void Stop();
};

//@section Actor
class AActor : public UObject
{
public:
	AActor();
	AActor(const FObjectInitializer& ObjectInitializer);
	FActorTickFunction PrimaryActorTick;
	TObjectPtr<USceneComponent> RootComponent;
	virtual void BeginPlay();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);
	virtual void Tick(float DeltaSeconds);
	template <typename TReturnType>
	TReturnType* CreateDefaultSubobject(FName SubobjectName, bool bTransient = false);
	void AddInstanceComponent(UActorComponent* Component);
	UGameInstance* GetGameInstance() const;
	template <typename T>
	T* GetGameInstance() const;
	virtual UWorld* GetWorld() const override;
};

//@section PlayerController: Actor
class AController : public AActor
{
};

class APawn : public AActor
{
};

struct FInputModeDataBase
{
	virtual ~FInputModeDataBase();
};

enum class EMouseLockMode : uint8
{
	DoNotLock,
	LockOnCapture,
	LockAlways,
	LockInFullscreen
};

struct FInputModeUIOnly : public FInputModeDataBase
{
	FInputModeUIOnly();
	FInputModeUIOnly& SetWidgetToFocus(TSharedPtr<SWidget> InWidgetToFocus);
	FInputModeUIOnly& SetLockMouseToViewportBehavior(EMouseLockMode InMouseLockMode);
};

class APlayerController : public AController
{
public:
	uint32 bShowMouseCursor : 1;
	uint32 bEnableClickEvents : 1;
	uint32 bEnableTouchEvents : 1;
	uint32 bAutoManageActiveCameraTarget : 1;
	bool DeprojectScreenPositionToWorld(float ScreenX, float ScreenY, FVector& WorldLocation, FVector& WorldDirection) const;
	bool ProjectWorldLocationToScreen(FVector WorldLocation, FVector2D& ScreenLocation, bool bPlayerViewportRelative = false) const;
	void GetViewportSize(int32& SizeX, int32& SizeY) const;
	virtual void SetViewTarget(AActor* NewViewTarget);
	virtual void SetInputMode(const FInputModeDataBase& InData);
	virtual void ActivateTouchInterface(UTouchInterface* NewTouchInterface);
	bool IsLocalController() const;
};

//@section GameModeBase: Actor
class AGameModeBase : public AActor
{
public:
	TSubclassOf<APawn> DefaultPawnClass;
	TSubclassOf<APlayerController> PlayerControllerClass;
	TSubclassOf<AHUD> HUDClass;
};

//@section Font: CoreUObject
class UFont : public UObject
{
};

//@section CanvasItem: Core CoreUObject EngineFwd
struct FCanvasItem
{
	virtual ~FCanvasItem();
};

class FCanvasTextItem : public FCanvasItem
{
public:
	FCanvasTextItem(const FVector2D& InPosition, const FText& InText, const UFont* InFont, const FLinearColor& InColor);
	FVector2D Scale;
	uint32 bCentreX : 1;
	uint32 bCentreY : 1;
	void EnableShadow(const FLinearColor& InColor, const FVector2D& InOffset = FVector2D(1.0, 1.0));
};

//@section Canvas: CoreUObject EngineFwd
class UCanvas : public UObject
{
public:
	float ClipX;
	float ClipY;
	void DrawItem(FCanvasItem& Item);
};

//@section HUD: Actor
class AHUD : public AActor
{
public:
	TObjectPtr<UCanvas> Canvas;
	TObjectPtr<APlayerController> PlayerOwner;
	virtual void DrawHUD();
	FVector Project(FVector Location, bool bClampToZeroPlane = true) const;
	void DrawRect(FLinearColor RectColor, float ScreenX, float ScreenY, float ScreenW, float ScreenH);
	void DrawLine(float StartScreenX, float StartScreenY, float EndScreenX, float EndScreenY, FLinearColor LineColor, float LineThickness = 0.f);
};

//@section Engine: CoreUObject EngineFwd
class UEngine : public UObject
{
public:
	static UFont* GetSmallFont();
	static UFont* GetMediumFont();
	static UFont* GetLargeFont();
};
extern UEngine* GEngine;

//@section GameInstance: CoreUObject EngineFwd
class UGameInstance : public UObject
{
public:
	virtual void Init();
	virtual void Shutdown();
};

//@section GameViewportClient: CoreUObject EngineFwd
class FEngineShowFlags
{
public:
	void SetTonemapper(bool bVisible);
	void SetEyeAdaptation(bool bVisible);
	void SetBloom(bool bVisible);
};

class UGameViewportClient : public UObject
{
public:
	FEngineShowFlags EngineShowFlags;
	virtual void AddViewportWidgetContent(TSharedRef<SWidget> ViewportContent, const int32 ZOrder = 0);
	virtual void RemoveViewportWidgetContent(TSharedRef<SWidget> ViewportContent);
};

//@section World: CoreUObject EngineFwd
enum class ESpawnActorCollisionHandlingMethod : uint8
{
	Undefined,
	AlwaysSpawn,
	AdjustIfPossibleButAlwaysSpawn,
	AdjustIfPossibleButDontSpawnIfColliding,
	DontSpawnIfColliding
};

struct FActorSpawnParameters
{
	FActorSpawnParameters();
	ESpawnActorCollisionHandlingMethod SpawnCollisionHandlingOverride;
};

class UWorld : public UObject
{
public:
	template <typename T>
	T* SpawnActor(UClass* Class, const FTransform& Transform, const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters());
	UGameViewportClient* GetGameViewport() const;
	template <typename T>
	T* GetAuthGameMode() const;
	double GetRealTimeSeconds() const;
	double GetTimeSeconds() const;
};

//@section SaveGame: CoreUObject
class USaveGame : public UObject
{
};

//@section BlueprintFunctionLibrary: CoreUObject
class UBlueprintFunctionLibrary : public UObject
{
};

//@section GameplayStatics: BlueprintFunctionLibrary EngineFwd
class UGameplayStatics : public UBlueprintFunctionLibrary
{
public:
	static bool DoesSaveGameExist(const FString& SlotName, const int32 UserIndex);
	static USaveGame* LoadGameFromSlot(const FString& SlotName, const int32 UserIndex);
	static bool SaveGameToSlot(USaveGame* SaveGameObject, const FString& SlotName, const int32 UserIndex);
	static bool DeleteGameInSlot(const FString& SlotName, const int32 UserIndex);
	static USaveGame* CreateSaveGameObject(TSubclassOf<USaveGame> SaveGameClass);
};

//@section KismetSystemLibrary: BlueprintFunctionLibrary EngineFwd
namespace EQuitPreference
{
enum Type
{
	Quit,
	Background
};
}

class UKismetSystemLibrary : public UBlueprintFunctionLibrary
{
public:
	static void QuitGame(const UObject* WorldContextObject, APlayerController* SpecificPlayer, TEnumAsByte<EQuitPreference::Type> QuitPreference, bool bIgnorePlatformRestrictions);
};

//@section DeveloperSettings: CoreUObject
class UDeveloperSettings : public UObject
{
public:
	UDeveloperSettings();
	UDeveloperSettings(const FObjectInitializer& ObjectInitializer);
};

