#include <iostream>
#include <type_traits>

namespace standard {
   namespace detail {
      void tag_invoke();
      struct tag_invoke_t {
         template<typename Tag, typename... Args>
         constexpr auto operator() (Tag tag, Args &&... args) const
            noexcept(noexcept(tag_invoke(static_cast<Tag &&>(tag), static_cast<Args &&>(args)...)))
            -> decltype(tag_invoke(static_cast<Tag &&>(tag), static_cast<Args &&>(args)...)) {
            return tag_invoke(static_cast<Tag &&>(tag), static_cast<Args &&>(args)...);
         }
      };
   }

   inline constexpr detail::tag_invoke_t tag_invoke{};
   
   template<auto& Tag> 
   using tag_t = std::decay_t<decltype(Tag)>; 
}
