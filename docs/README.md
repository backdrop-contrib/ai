# AI Module Documentation

AI connects Backdrop features to separately installed provider modules. A
provider supplies one or more models. Each model supports specific operations,
such as chat, image generation, vision, or embeddings. The provider's PHP
adapter translates shared API requests into that service's format.

Start with the [AI README](../README.md) to connect a provider and select
default models. The [project wiki](https://github.com/backdrop-contrib/ai/wiki)
covers setup and use of the bundled features.

## Admin and developer references

- [Provider architecture](PROVIDERS.md)
- [Privacy and data handling](privacy.md)
- [Rate limiting](rate-limiting.md)
- [Shared site context](site-context.md)
- [HTML to Markdown for AI context](html-to-markdown.md)
- [Background jobs with AI Async](ai-async.md)
- [Coding style](AI_STYLE.md)

The [example provider](../examples/README.md) demonstrates registration,
configuration, model discovery, and adapter behavior with local sample data.
