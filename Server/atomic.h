#pragma once
#include <atomic>
#include <type_traits>

// 보통 템플릿인 외부에서 인자로 타입을 넣어주지만, 최근에는 자동 추론도 해준다고 한다.
template <class T>
class Atomic {
    static_assert(std::is_trivially_copyable_v<T>,
        "AtomicField<T> requires trivially copyable T.");

private:
    std::atomic<T> value;

public:
    using value_type = T; 
    // 템플릿 클래스는 외부에서 어떤 타입으로 동작하는지 바로 알 수 없으므로,
    // 내부에서 다루는 실제 타입을 value_type이라는 이름으로 노출한다.
    // 이 타입은 템플릿이 특수화될 때(예: Box<int>) 결정된다.
    // C++17 이후에는 타입을 지정하지 않아도 자동 추론해준다.
    
    // <제네릭 프로그래밍>에서는 컨테이너의 원소 타입을 외부에서 공통된 규칙(value_type)으로 접근할 수 있어야 한다.
    // 따라서 STL 컨테이너와의 호환성이나 범용 알고리즘과 함께 쓰려면 꼭 필요하다.
    
    // ※ 제네릭 프로그래밍은 자료형에 의존하지 않고 여러 자료형으로 재사용 가능하도록 작성하는 프로그래밍을 말한다.
    // 즉 여러 자료형을 넣어도 다 사용 가능한 코드를 말한다.

    constexpr Atomic() noexcept : value{} {} // 기본 생성자 -> std::atomic의 기본 생성자를 호출함
    constexpr Atomic(T init) noexcept : value(init) {} // 오버로드된 생성자

    // 복사/이동 생성자 -> 사실 사용하지는 않을 예정이지만, vector에 들어가려면 필요
    Atomic(const Atomic& other) noexcept {
        value.store(other.value.load());
    }
    Atomic(Atomic&& other) noexcept {
        value.store(other.value.load());
    }

    Atomic& operator=(const T& desired) noexcept {
        value.store(desired);
        return *this;
    }

    bool operator==(const T& desired) noexcept {
		return value.load() == desired;
    }
    // const T&은 임시 객체를 받을 수 있다. 임시 객체는 a = 1에서 1을 의미하는데, 잠시만 생성되었다가 삭제되는 값이다. 일반 T&은 임시 객체를 받을 수 없는데, 임시 객체가 삭제되어 
	// 참조가 끊어질 수 있고,댕글링 참조를 수정할 가능성이 있기 때문이다. const가 붙으면 수정할 일이 없고, 임시 객체를 참조중인 메모리가 사라질 때 까지 유지된다.
    // 즉 위의 const T& desired의 desired가 임시 객체라면 return 후 소멸한다.

    //Atomic& operator=(const Atomic& other) noexcept {
    //    if (this != &other)
    //        value.store(other.value.load());
    //    return *this;
    //}

    //Atomic& operator=(Atomic&& other) noexcept {
    //    if (this != &other)
    //        value.store(other.value.load());
    //    return *this;
    //}

    T Load() const noexcept {
        return value.load();
    }
    void Store(T desired) noexcept {
        value.store(desired);
    }


    bool Compare_exchange_weak(T& expected, T desired) noexcept {
        return value.compare_exchange_weak(expected, desired);
    }
    bool Compare_exchange_strong(T& expected, T desired) noexcept {
        return value.compare_exchange_strong(expected, desired);
    }

    T GetSelf() const noexcept {
        return value.load();
    }
};

